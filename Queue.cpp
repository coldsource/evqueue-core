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

#include <Queue.h>
#include <QueuePool.h>
#include <Logger.h>
#include <WorkflowInstance.h>
#include <Exception.h>

#include <string.h>
#include <stdio.h>
#include <ctype.h>

Queue::Queue(const char *name, int concurrency, int scheduler)
{
	if(!CheckQueueName(name))
		throw Exception("Queue","Invalid queue name");
	
	strcpy(this->name,name);
	
	this->concurrency = concurrency;
	this->scheduler = scheduler;
	
	size = 0;
	running_tasks = 0;
	
	removed = false;
}

Queue::~Queue()
{
	if(scheduler==QUEUE_SCHEDULER_FIFO)
	{
		for(auto it=queue.begin();it!=queue.end();it++)
			delete (*it);
	}
	else if(scheduler==QUEUE_SCHEDULER_PRIO)
	{
		for(auto it=prio_queue.begin();it!=prio_queue.end();it++)
			delete it->second;
	}
}

bool Queue::CheckQueueName(const char *queue_name)
{
	int i,len;
	
	len = strlen(queue_name);
	if(len==0 || len>QUEUE_NAME_MAX_LEN)
		return false;
	
	for(i=0;i<len;i++)
		if(!isalnum(queue_name[i]) && queue_name[i]!='_' && queue_name[i]!='-')
			return false;
	
	return true;
}

void Queue::EnqueueTask(WorkflowInstance *workflow_instance,DOMNode *task)
{
	Task *new_task = new Task;
	new_task->workflow_instance = workflow_instance;
	new_task->task = task;
	
	if(scheduler==QUEUE_SCHEDULER_FIFO)
		queue.push_back(new_task);
	else if(scheduler==QUEUE_SCHEDULER_PRIO)
		prio_queue.insert(std::pair<unsigned int,Task *>(workflow_instance->GetInstanceID(),new_task));
	
	size++;
}

bool Queue::DequeueTask(WorkflowInstance **p_workflow_instance,DOMNode **p_task)
{
	if(IsLocked())
		return false;
	
	Task *tmp;
	
	// If we have cancelled tasks, return it first
	if(cancelled_tasks.size())
	{
		// Dequeue cancelled task
		tmp = cancelled_tasks.front();
		cancelled_tasks.pop();
	}
	else
	{
		// Dequeue task
		if(scheduler==QUEUE_SCHEDULER_FIFO)
		{
			tmp = queue.front();
			queue.pop_front();
		}
		else if(scheduler==QUEUE_SCHEDULER_PRIO)
		{
			std::multimap<unsigned int,Task *>::iterator it;
			it=prio_queue.begin();
			tmp = it->second;
			prio_queue.erase(it);
		}
		
		size--;
	}
	
	// Retreive task data
	*p_workflow_instance = tmp->workflow_instance;
	*p_task = tmp->task;
	
	delete tmp;
	
	return true;
}

bool Queue::ExecuteTask(void)
{
	running_tasks++;
	return true;
}


bool Queue::TerminateTask(void)
{
	running_tasks--;
	return true;
}

bool Queue::CancelTasks(unsigned int workflow_instance_id)
{
	if(scheduler==QUEUE_SCHEDULER_FIFO)
	{
		for(auto it=queue.begin();it!=queue.end();)
		{
			if((*it)->workflow_instance->GetInstanceID()==workflow_instance_id)
			{
				cancelled_tasks.push((*it));
				it = queue.erase(it);
				
				size--;
			}
			else
				++it;
		}
	}
	else if(scheduler==QUEUE_SCHEDULER_PRIO)
	{
		for(auto it=prio_queue.cbegin();it!=prio_queue.cend();)
		{
			if(it->second->workflow_instance->GetInstanceID()==workflow_instance_id)
			{
				cancelled_tasks.push(it->second);
				prio_queue.erase(it++);
				
				size--;
			}
			else
				++it;
		}
	}
	
	return true;
}

void Queue::SetConcurrency(unsigned int concurrency)
{
	this->concurrency = concurrency;
	removed = false;
}

void Queue::SetScheduler(unsigned int new_scheduler)
{
	if(new_scheduler==scheduler)
		return;
	
	Logger::Log(LOG_NOTICE,"[ Queue ] %s : migrating scheduler to %s",name,QueuePool::get_scheduler_from_int(new_scheduler).c_str());
	
	if(new_scheduler==QUEUE_SCHEDULER_FIFO)
	{
		for(auto it=prio_queue.begin();it!=prio_queue.end();it++)
			queue.push_back(it->second);
		prio_queue.clear();
	}
	
	if(new_scheduler==QUEUE_SCHEDULER_PRIO)
	{
		for(auto it=queue.begin();it!=queue.end();it++)
			prio_queue.insert(std::pair<unsigned int,Task *>((*it)->workflow_instance->GetInstanceID(),(*it)));
		queue.clear();
	}
	
	scheduler = new_scheduler;
}

bool Queue::IsLocked(void)
{
	if(cancelled_tasks.size())
		return false; // Always return cancelled tasks
	if(size>0 && running_tasks<concurrency)
		return false; // Return new tasks only if concurrency is not reached
	return true;
}
