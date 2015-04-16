/*
 * This file is part of evQueue
 * 
 * evQueue is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * evQueue is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with evQueue. If not, see <http://www.gnu.org/licenses/>.
 * 
 * Author: Thibault Kummer <bob@coldsource.net>
 */

#include <QueuePool.h>
#include <Queue.h>
#include <DB.h>
#include <Exception.h>
#include <WorkflowInstance.h>
#include <Logger.h>

#include <xqilla/xqilla-dom3.hpp>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>

QueuePool *QueuePool::instance = 0;

QueuePool::QueuePool(void)
{
	int i;
	DB db;
	
	name = 0;
	queue = 0;
	
	db.Query("SELECT queue_name FROM t_queue ORDER BY queue_name ASC");
	pool_size = db.NumRows();
	
	name = new char *[pool_size];
	queue = new Queue *[pool_size];
	for(i=0;i<pool_size;i++)
	{
		name[i] = 0;
		queue[i] = 0;
	}
	
	i = 0;
	while(db.FetchRow())
	{
		name[i] = new char[strlen(db.GetField(0))+1];
		strcpy(name[i],db.GetField(0));
		queue[i] = new Queue(name[i]);
		
		i++;
	}
	
	FILE *f;
	char buf[10];
	
	f = fopen("/proc/sys/kernel/pid_max","r");
	if(!f)
		throw Exception("QueuePool","Unable to read /proc/sys/kernel/pid_max");
	
	fread(buf,1,10,f);
	fclose(f);
	
	maxpid = atoi(buf);
	lasttid = 0;
	
	tid_queue_index = new unsigned int[maxpid+1];
	tid_task = new DOMNode*[maxpid+1];
	tid_workflow_instance = new WorkflowInstance*[maxpid+1];
	
	for(int i=0;i<=maxpid;i++)
	{
		tid_queue_index[i] = 0;
		tid_task[i] = 0;
		tid_workflow_instance[i] = 0;
	}
	
	is_shutting_down = false;
	
	pthread_mutex_init(&mutex,NULL);
	pthread_cond_init(&fork_lock,NULL);
	
	pthread_mutex_lock(&mutex);
	fork_possible = !IsLocked();
	pthread_mutex_unlock(&mutex);
	
	fork_locked = false;
	
	instance = this;
}

QueuePool::~QueuePool(void)
{
	int i;
	
	if(name)
	{
		for(i=0;i<pool_size;i++)
			if(name[i]!=0)
				delete[] name[i];
		delete[] name;
	}
	
	if(queue)
	{
		for(i=0;i<pool_size;i++)
			if(queue[i]!=0)
				delete queue[i];
		delete[] queue;
	}
	
	delete[] tid_queue_index;
	delete[] tid_task;
	delete[] tid_workflow_instance;
}

int QueuePool::lookup(const char *queue_name)
{
	int cmp,begin,middle,end;
	
	begin=0;
	end=pool_size-1;
	while(begin<=end)
	{
		middle=(end+begin)/2;
		cmp=strcmp(name[middle],queue_name);
		if(cmp==0)
			return middle;
		if(cmp<0)
			begin=middle+1;
		else
			end=middle-1;
	}
	
	return -1;
}

Queue *QueuePool::GetQueue(const char *queue_name)
{
	int i;
	
	i = lookup(queue_name);
	if(i!=-1)
		return queue[i];
	
	return 0;
}

bool QueuePool::EnqueueTask(const char *queue_name,WorkflowInstance *workflow_instance,DOMNode *task)
{
	int i;
	
	pthread_mutex_lock(&mutex);
	
	i = lookup(queue_name);
	if(i==-1)
	{
		pthread_mutex_unlock(&mutex);
		return false;
	}
	
	queue[i]->EnqueueTask(workflow_instance,task);
	
	fork_possible = !IsLocked();
	if(fork_locked && fork_possible)
		pthread_cond_signal(&fork_lock);
	pthread_mutex_unlock(&mutex);
	
	return true;
}

bool QueuePool::DequeueTask(char *queue_name,WorkflowInstance **p_workflow_instance,DOMNode **p_task)
{
	int i;
	unsigned int task_instance_id;
	
	pthread_mutex_lock(&mutex);
	
	if(is_shutting_down)
	{
		pthread_mutex_unlock(&mutex); // Shutdown in progress juste exit
		return false;
	}
	
	if(!fork_possible)
	{
		fork_locked = true;
		pthread_cond_wait(&fork_lock,&mutex);
	}
	
	if(is_shutting_down)
	{
		pthread_mutex_unlock(&mutex); // Shutdown in progress juste exit
		return false;
	}
	
	fork_locked = false;
	
	for(i=0;i<pool_size;i++)
	{
		if(queue[i]->DequeueTask(p_workflow_instance,p_task))
		{
			strcpy(queue_name,name[i]);
			
			fork_possible = !IsLocked();
		
			pthread_mutex_unlock(&mutex);
			
			return true;
		}
	}
	
	pthread_mutex_unlock(&mutex);
	
	return false;
}

pid_t QueuePool::ExecuteTask(WorkflowInstance *workflow_instance,DOMNode *task,const char *queue_name,pid_t task_id)
{
	int i;
	
	i = lookup(queue_name);
	if(i==-1)
		return 0;
	
	pthread_mutex_lock(&mutex);
	
	if(task_id==0)
	{
		unsigned int i;
		for(i=0;i<maxpid;i++)
			if(tid_workflow_instance[task_id=(lasttid+1+i)%maxpid]==0)
				break;
			
		lasttid = task_id;
		if(i==maxpid)
		{
			Logger::Log(LOG_ERR,"Could not allocate task ID, maxpid (%d) is reached",maxpid);
			
			pthread_mutex_unlock(&mutex);
			return 0; // Could not allocate tid
		}
	}
	
	tid_queue_index[task_id] = i;
	tid_task[task_id] = task;
	tid_workflow_instance[task_id] = workflow_instance;
	
	queue[i]->ExecuteTask();
	
	fork_possible = !IsLocked();
	if(fork_locked && fork_possible)
		pthread_cond_signal(&fork_lock);
	
	pthread_mutex_unlock(&mutex);
	
	return task_id;
}

bool QueuePool::TerminateTask(pid_t task_id,WorkflowInstance **p_workflow_instance,DOMNode **p_task)
{
	pthread_mutex_lock(&mutex);
	
	if(tid_workflow_instance[task_id]==0)
	{
		// Task not found
		pthread_mutex_unlock(&mutex);
		return false;
	}
	
	queue[tid_queue_index[task_id]]->TerminateTask();
	
	*p_workflow_instance = tid_workflow_instance[task_id];
	*p_task = tid_task[task_id];
	
	tid_queue_index[task_id] = 0;
	tid_task[task_id] = 0;
	tid_workflow_instance[task_id] = 0;
	
	fork_possible = !IsLocked();
	if(fork_locked && fork_possible)
		pthread_cond_signal(&fork_lock);
	
	pthread_mutex_unlock(&mutex);
	
	return true;
}

bool QueuePool::CancelTasks(unsigned int workflow_instance_id)
{
	pthread_mutex_lock(&mutex);
	
	for(int i=0;i<pool_size;i++)
		queue[i]->CancelTasks(workflow_instance_id);
	
	fork_possible = !IsLocked();
	if(fork_locked && fork_possible)
		pthread_cond_signal(&fork_lock);
	
	pthread_mutex_unlock(&mutex);
	
	return true;
}

bool QueuePool::IsLocked(void)
{
	int i;
	for(i=0;i<pool_size;i++)
		if(!queue[i]->IsLocked())
			return false;
	return true;
}

void QueuePool::Shutdown(void)
{
	pthread_mutex_lock(&mutex);
	
	is_shutting_down = true;
	
	if(fork_locked)
		pthread_cond_signal(&fork_lock);
	
	pthread_mutex_unlock(&mutex);
}

void QueuePool::SendStatistics(int s)
{
	char buf[16];
	
	DOMImplementation *xqillaImplementation = DOMImplementationRegistry::getDOMImplementation(X("XPath2 3.0"));
	DOMDocument *xmldoc = xqillaImplementation->createDocument();
	
	DOMElement *statistics_node = xmldoc->createElement(X("statistics"));
	xmldoc->appendChild(statistics_node);
	
	for(int i=0;i<pool_size;i++)
	{
		DOMElement *queue_node = xmldoc->createElement(X("queue"));
		queue_node->setAttribute(X("name"),X(name[i]));
		
		sprintf(buf,"%d",queue[i]->GetConcurrency());
		queue_node->setAttribute(X("concurrency"),X(buf));
		
		sprintf(buf,"%d",queue[i]->GetSize());
		queue_node->setAttribute(X("size"),X(buf));
		
		sprintf(buf,"%d",queue[i]->GetRunningTasks());
		queue_node->setAttribute(X("running_tasks"),X(buf));
		
		statistics_node->appendChild(queue_node);
	}
	
	DOMLSSerializer *serializer = xqillaImplementation->createLSSerializer();
	XMLCh *statistics_xml = serializer->writeToString(statistics_node);
	char *statistics_xml_c = XMLString::transcode(statistics_xml);
	
	send(s,statistics_xml_c,strlen(statistics_xml_c),0);
	
	XMLString::release(&statistics_xml);
	XMLString::release(&statistics_xml_c);
	serializer->release();
	xmldoc->release();
}
