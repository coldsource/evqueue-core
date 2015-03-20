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
#include <DB.h>
#include <WorkflowInstance.h>
#include <Configuration.h>
#include <Exception.h>

#include <string.h>
#include <stdio.h>
#include <ctype.h>

Queue::Queue(const char *name)
{
	if(!CheckQueueName(name))
		throw Exception("Queue","Invalid queue name");
	
	strcpy(this->name,name);
	
	DB db;
	
	db.QueryPrintf("SELECT queue_id,queue_concurrency FROM t_queue WHERE queue_name=%s",name);
	if(!db.FetchRow())
		throw Exception("Queue","Unknown queue");
	id = db.GetFieldInt(0);
	concurrency = db.GetFieldInt(1);
	
	size = 0;
	running_tasks = 0;
	first_task = 0;
	last_task = 0;
	cancelled_tasks = 0;
}

bool Queue::CheckQueueName(const char *queue_name)
{
	int i,len;
	
	len = strlen(queue_name);
	if(len>QUEUE_NAME_MAX_LEN)
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
	new_task->next_task = 0;
	
	if(first_task==0)
	{
		last_task = new_task;
		first_task = new_task;
	}
	else
	{
		last_task->next_task = new_task;
		last_task = new_task;
	}
	
	size++;
}

bool Queue::DequeueTask(WorkflowInstance **p_workflow_instance,DOMNode **p_task)
{
	Task *tmp;
	
	if(IsLocked())
		return false;
	
	// If we have cancelled tasks, return it first
	if(cancelled_tasks)
	{
		// Dequeue cancelled task
		tmp = cancelled_tasks;
		cancelled_tasks = cancelled_tasks->next_task;
	}
	else
	{
		// Dequeue task
		tmp = first_task;
		first_task = first_task->next_task;
		
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
	Task *tmp;
	Task **task = &first_task;
	
	while(*task)
	{
		if((*task)->workflow_instance->GetInstanceID()==workflow_instance_id)
		{
			tmp = *task;
			*task = (*task)->next_task;
			
			tmp->next_task = cancelled_tasks;
			cancelled_tasks = tmp;
			
			size--;
		}
		else
			task = &((*task)->next_task);
	}
	
	return true;
}

bool Queue::IsLocked(void)
{
	if(cancelled_tasks)
		return false; // Always return cancelled tasks
	if(size>0 && running_tasks<concurrency)
		return false; // Return new tasks only if concurrency is not reached
	return true;
}
