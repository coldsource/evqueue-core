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
#include <SocketQuerySAX2Handler.h>
#include <QueryResponse.h>
#include <Cluster.h>
#include <User.h>

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
	std::string scheduler_str = Configuration::GetInstance()->Get("queuepool.scheduler");
	default_scheduler = get_scheduler_from_string(scheduler_str);
	
	Logger::Log(LOG_NOTICE,"Loaded default scheduler '%s'",scheduler_str.c_str());
	
	FILE *f;
	char buf[10];
	
	f = fopen("/proc/sys/kernel/pid_max","r");
	if(!f)
		throw Exception("QueuePool","Unable to open /proc/sys/kernel/pid_max");
	
	if(fread(buf,1,10,f)<=0)
		throw Exception("QueuePool","Unable to read /proc/sys/kernel/pid_max");
	
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
	
	Reload(false);
}

QueuePool::~QueuePool(void)
{
	for(auto it=queues_name.begin();it!=queues_name.end();++it)
		delete it->second;
	
	queues_name.clear();
	queues_id.clear();
	
	delete[] tid_queue;
	delete[] tid_task;
	delete[] tid_workflow_instance;
}

bool QueuePool::EnqueueTask(const string &queue_name,WorkflowInstance *workflow_instance,DOMNode *task)
{
	pthread_mutex_lock(&mutex);
	
	Queue *q = get_queue(queue_name);
	
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
	for(it=queues_name.begin();it!=queues_name.end();++it)
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
	Queue *q = get_queue(queue_name);
	
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
	
	Queue *q = tid_queue[task_id];
	q->TerminateTask();
	
	*p_workflow_instance = tid_workflow_instance[task_id];
	*p_task = tid_task[task_id];
	
	tid_queue[task_id] = 0;
	tid_task[task_id] = 0;
	tid_workflow_instance[task_id] = 0;
	
	// Check if queue has been marked as 'to be removed'
	if(q->IsRemoved() && q->GetSize()==0 && q->GetRunningTasks()==0)
	{
		Logger::Log(LOG_NOTICE,"Removed queue %s",q->GetName().c_str());
		
		queues_name.erase(q->GetName());
		delete q;
	}
	
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
	for(it=queues_name.begin();it!=queues_name.end();++it)
		it->second->CancelTasks(workflow_instance_id);
	
	fork_possible = !IsLocked();
	if(fork_locked && fork_possible)
		pthread_cond_signal(&fork_lock);
	
	pthread_mutex_unlock(&mutex);
	
	return true;
}

void QueuePool::Reload(bool notify)
{
	DB db;
	
	Logger::Log(LOG_NOTICE,"Reloading queues definitions");
	
	pthread_mutex_lock(&mutex);
	
	// First, remove empty queues and mark others as to be removed
	for(auto it=queues_name.cbegin();it!=queues_name.cend();)
	{
		if(it->second->GetSize()==0 && it->second->GetRunningTasks()==0)
		{
			queues_id.erase(it->second->GetID());
			delete it->second;
			queues_name.erase(it++);
		}
		else
		{
			it->second->Remove();
			++it;
		}
	}
	
	// Reload new parameters or re-create removed queues
	db.Query("SELECT queue_id,queue_name,queue_concurrency,queue_scheduler FROM t_queue");
	
	while(db.FetchRow())
	{
		Queue *q = get_queue(db.GetField(1));
		if(!q)
		{
			q = new Queue(db.GetFieldInt(0),db.GetField(1),db.GetFieldInt(2),get_scheduler_from_string(db.GetField(3)),db.GetField(3));
			queues_id[db.GetFieldInt(0)] = q;
			queues_name[db.GetField(1)] = q;
		}
		else
		{
			q->SetConcurrency(db.GetFieldInt(2));
			q->SetScheduler(get_scheduler_from_string(db.GetField(3)));
		}
	}
	
	// Update locked status as concurrencies might have changed
	fork_possible = !IsLocked();
	if(fork_locked && fork_possible)
		pthread_cond_signal(&fork_lock);
	
	pthread_mutex_unlock(&mutex);
	
	if(notify)
	{
		// Notify cluster
		Cluster::GetInstance()->ExecuteCommand("<control action='reload' module='queuepool' notify='no' />\n");
	}
}

bool QueuePool::IsLocked(void)
{
	int i;
	for(auto it=queues_name.begin();it!=queues_name.end();++it)
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

void QueuePool::SendStatistics(QueryResponse *response)
{
	char buf[16];
	
	pthread_mutex_lock(&mutex);
	
	DOMDocument *xmldoc = response->GetDOM();
	
	DOMElement *statistics_node = xmldoc->createElement(X("statistics"));
	xmldoc->getDocumentElement()->appendChild(statistics_node);
	
	for(auto it=queues_name.begin();it!=queues_name.end();++it)
	{
		DOMElement *queue_node = xmldoc->createElement(X("queue"));
		queue_node->setAttribute(X("name"),X(it->first.c_str()));
		
		sprintf(buf,"%d",it->second->GetConcurrency());
		queue_node->setAttribute(X("concurrency"),X(buf));
		
		sprintf(buf,"%d",it->second->GetSize());
		queue_node->setAttribute(X("size"),X(buf));
		
		sprintf(buf,"%d",it->second->GetRunningTasks());
		queue_node->setAttribute(X("running_tasks"),X(buf));
		
		queue_node->setAttribute(X("scheduler"),X(get_scheduler_from_int(it->second->GetScheduler()).c_str()));
		
		statistics_node->appendChild(queue_node);
	}
	
	pthread_mutex_unlock(&mutex);
}

void QueuePool::GetQueue(unsigned int id, QueryResponse *response)
{
	QueuePool *qp = QueuePool::GetInstance();
	
	pthread_mutex_lock(&qp->mutex);
	
	Queue *q = qp->get_queue(id);
	if(!q)
	{
		pthread_mutex_unlock(&qp->mutex);
		
		throw Exception("QueuePool","Unable to find queue");
	}
	
	DOMElement *node = (DOMElement *)response->AppendXML("<queue />");
	node->setAttribute(X("id"),X(to_string(q->GetID()).c_str()));
	node->setAttribute(X("name"),X(q->GetName().c_str()));
	node->setAttribute(X("concurrency"),X(to_string(q->GetConcurrency()).c_str()));
	node->setAttribute(X("scheduler"),X(q->GetWantedScheduler().c_str()));
	
	pthread_mutex_unlock(&qp->mutex);
}

bool QueuePool::Exists(unsigned int id)
{
	pthread_mutex_lock(&mutex);
	
	Queue *q = get_queue(id);
	if(!q)
	{
		pthread_mutex_unlock(&mutex);
		
		return false;
	}
	
	pthread_mutex_unlock(&mutex);
	
	return true;
}

bool QueuePool::HandleQuery(const User &user, SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	QueuePool *qp = QueuePool::GetInstance();
	
	const string action = saxh->GetRootAttribute("action");
	
	if(action=="list")
	{
		pthread_mutex_lock(&qp->mutex);
		
		for(auto it = qp->queues_name.begin(); it!=qp->queues_name.end(); it++)
		{
			DOMElement *node = (DOMElement *)response->AppendXML("<queue />");
			node->setAttribute(X("id"),X(std::to_string(it->second->GetID()).c_str()));
			node->setAttribute(X("name"),X(it->second->GetName().c_str()));
			node->setAttribute(X("concurrency"),X(to_string(it->second->GetConcurrency()).c_str()));
			node->setAttribute(X("scheduler"),X(it->second->GetWantedScheduler().c_str()));
		}
		
		pthread_mutex_unlock(&qp->mutex);
		
		return true;
	}
	
	return false;
}

Queue *QueuePool::get_queue(unsigned int id)
{
	auto it = queues_id.find(id);
	if(it==queues_id.end())
		return 0;
	return it->second;
}

Queue *QueuePool::get_queue(const string &name)
{
	auto it = queues_name.find(name);
	if(it==queues_name.end())
		return 0;
	return it->second;
}

int QueuePool::get_scheduler_from_string(std::string scheduler_str)
{
	if(scheduler_str=="default")
		return default_scheduler;
	else if(scheduler_str=="fifo")
		return QUEUE_SCHEDULER_FIFO;
	else if(scheduler_str=="prio")
		return QUEUE_SCHEDULER_PRIO;
	else
		throw Exception("QueuePool","Invalid scheduler name, allowed values are 'fifo' or 'prio'");
}

std::string QueuePool::get_scheduler_from_int(int scheduler)
{
	if(scheduler==QUEUE_SCHEDULER_FIFO)
		return "fifo";
	else if(scheduler==QUEUE_SCHEDULER_PRIO)
		return "prio";
	else
		return "";
}
