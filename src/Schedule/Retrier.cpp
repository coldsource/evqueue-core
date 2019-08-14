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

#include <Schedule/Retrier.h>
#include <WorkflowInstance/WorkflowInstance.h>
#include <Logger/Logger.h>

#include <global.h>

#include <stdio.h>
#include <unistd.h>
#include <time.h>

Retrier *Retrier::instance = 0;

using namespace std;

Retrier::Retrier(): Scheduler()
{
	self_name = "Retrier";
	instance = this;
}

void Retrier::InsertTask(WorkflowInstance *workflow_instance,DOMElement task, time_t retry_at)
{
	TimedTask *new_task = new TimedTask;
	new_task->workflow_instance = workflow_instance;
	new_task->task = task;
	new_task->scheduled_at = retry_at;
	
	InsertEvent(new_task);
}

void Retrier::FlushWorkflowInstance(unsigned int workflow_instance_id)
{
	Logger::Log(LOG_NOTICE,"%s : Flushing tasks of workflow instance %d",self_name,workflow_instance_id);
	
	unique_lock<mutex> llock(retrier_lock);
	
	filter_workflow_instance_id = workflow_instance_id;
	
	Flush(true);
}

void Retrier::event_removed(Event *e, event_reasons reason)
{
	TimedTask *task = (TimedTask *)e;
	bool workflow_terminated;
	
	task->workflow_instance->TaskRestart(task->task,&workflow_terminated);
	
	if(workflow_terminated)
		delete task->workflow_instance;
	
	delete task;
}

bool Retrier::flush_filter(Event *e)
{
	TimedTask *task = (TimedTask *)e;
	if(task->workflow_instance->GetInstanceID()==filter_workflow_instance_id)
		return true;
	return false;
}
