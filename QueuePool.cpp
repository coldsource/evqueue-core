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
#include <Configuration.h>

#include <xqilla/xqilla-dom3.hpp>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>

#include <sys/types.h>
#include <sys/socket.h>

QueuePool *QueuePool::instance = 0;

using namespace std;

QueuePool::QueuePool(void)
{
	int i;
	DB db;
	
	std::string scheduler_str = Configuration::GetInstance()->Get("queuepool.scheduler");
	int scheduler;
	if(scheduler_str=="fifo")
		scheduler = QUEUE_SCHEDULER_FIFO;
	else if(scheduler_str=="prio")
		scheduler = QUEUE_SCHEDULER_PRIO;
	else
		throw Exception("QueuePool","Invalid scheduler name, allowed values are 'fifo' or 'prio'");
	
	Logger::Log(LOG_NOTICE,"[ QueuePool ] Loaded scheduler '%s'",scheduler_str.c_str());
	
	db.Query("SELECT queue_name FROM t_queue ORDER BY queue_name ASC");
	
	while(db.FetchRow())
		queues[db.GetField(0)] = new Queue(db.GetField(0),scheduler);
	
	FILE *f;
	char buf[10];
	
	f = fopen("/proc/sys/kernel/pid_max","r");
	if(!f)
		throw Exception("QueuePool","Unable to read /proc/sys/kernel/pid_max");
	
	fread(buf,1,10,f);
	fclose(f);
	
	maxpid = atoi(buf);
	lasttid = 0;
	
	tid_queue = new Queue*[maxpid+1];
	tid_task = new DOMNode*[maxpid+1];
	tid_workflow_instance = new WorkflowInstance*[maxpid+1];
	
	for(int i=0;i<=maxpid;i++)
	{
		tid_queue[i] = 0;
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
	map<string,Queue *>::iterator it;
	for(it=queues.begin();it!=queues.end();++it)
		delete it->second;
	
	queues.clear();
	
	delete[] tid_queue;
	delete[] tid_task;
	delete[] tid_workflow_instance;
}

Queue *QueuePool::GetQueue(const string &name)
{
	map<string,Queue *>::const_iterator it = queues.find(name);
	if(it==queues.end())
		return 0;
	return it->second;
}

bool QueuePool::EnqueueTask(const string &queue_name,WorkflowInstance *workflow_instance,DOMNode *task)
{
	pthread_mutex_lock(&mutex);
	
	Queue *q = GetQueue(queue_name);
	
	if(!q)
	{
		pthread_mutex_unlock(&mutex);
		return false;
	}
	
	q->EnqueueTask(workflow_instance,task);
	
	fork_possible = !IsLocked();
	if(fork_locked && fork_possible)
		pthread_cond_signal(&fork_lock);
	pthread_mutex_unlock(&mutex);
	
	return true;
}

bool QueuePool::DequeueTask(string &queue_name,WorkflowInstance **p_workflow_instance,DOMNode **p_task)
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
	
	map<string,Queue *>::iterator it;
	for(it=queues.begin();it!=queues.end();++it)
	{
		if(it->second->DequeueTask(p_workflow_instance,p_task))
		{
			queue_name = it->first;
			
			fork_possible = !IsLocked();
		
			pthread_mutex_unlock(&mutex);
			
			return true;
		}
	}
	
	pthread_mutex_unlock(&mutex);
	
	return false;
}

pid_t QueuePool::ExecuteTask(WorkflowInstance *workflow_instance,DOMNode *task,const string &queue_name,pid_t task_id)
{
	Queue *q = GetQueue(queue_name);
	
	if(!q)
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
	
	tid_queue[task_id] = q;
	tid_task[task_id] = task;
	tid_workflow_instance[task_id] = workflow_instance;
	
	q->ExecuteTask();
	
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
	
	tid_queue[task_id]->TerminateTask();
	
	*p_workflow_instance = tid_workflow_instance[task_id];
	*p_task = tid_task[task_id];
	
	tid_queue[task_id] = 0;
	tid_task[task_id] = 0;
	tid_workflow_instance[task_id] = 0;
	
	fork_possible = !IsLocked();
	if(fork_locked && fork_possible)
		pthread_cond_signal(&fork_lock);
	
	pthread_mutex_unlock(&mutex);
	
	return true;
}

bool QueuePool::GetTask(pid_t task_id,WorkflowInstance **p_workflow_instance,DOMNode **p_task)
{
	pthread_mutex_lock(&mutex);
	
	if(tid_workflow_instance[task_id]==0)
	{
		// Task not found
		pthread_mutex_unlock(&mutex);
		return false;
	}
	
	*p_workflow_instance = tid_workflow_instance[task_id];
	*p_task = tid_task[task_id];
	
	pthread_mutex_unlock(&mutex);
	
	return true;
}

bool QueuePool::CancelTasks(unsigned int workflow_instance_id)
{
	pthread_mutex_lock(&mutex);
	
	map<string,Queue *>::iterator it;
	for(it=queues.begin();it!=queues.end();++it)
		it->second->CancelTasks(workflow_instance_id);
	
	fork_possible = !IsLocked();
	if(fork_locked && fork_possible)
		pthread_cond_signal(&fork_lock);
	
	pthread_mutex_unlock(&mutex);
	
	return true;
}

bool QueuePool::IsLocked(void)
{
	int i;
	map<string,Queue *>::iterator it;
	for(it=queues.begin();it!=queues.end();++it)
		if(!it->second->IsLocked())
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
	
	map<string,Queue *>::iterator it;
	for(it=queues.begin();it!=queues.end();++it)
	{
		DOMElement *queue_node = xmldoc->createElement(X("queue"));
		queue_node->setAttribute(X("name"),X(it->first.c_str()));
		
		sprintf(buf,"%d",it->second->GetConcurrency());
		queue_node->setAttribute(X("concurrency"),X(buf));
		
		sprintf(buf,"%d",it->second->GetSize());
		queue_node->setAttribute(X("size"),X(buf));
		
		sprintf(buf,"%d",it->second->GetRunningTasks());
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
