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

#ifndef _RETRIER_H_
#define _RETRIER_H_

#include <Scheduler.h>

#include <DOMElement.h>

#include <mutex>

class WorkflowInstance;

class Retrier:public Scheduler
{
	private:
		struct TimedTask:Event
		{
			WorkflowInstance *workflow_instance;
			DOMElement task;
			
			virtual ~TimedTask() {}
		};
		
		static Retrier *instance;
		
		std::mutex retrier_lock;
		
		unsigned int filter_workflow_instance_id;
		
	public:
		Retrier();
		static Retrier *GetInstance() { return instance; }
		
		void InsertTask(WorkflowInstance *workflow_instance,DOMElement task, time_t retry_at );
		
		void FlushWorkflowInstance(unsigned int workflow_instance_id);
	
	protected:
		void event_removed(Event *e, event_reasons reason);
		
		bool flush_filter(Event *e);
};

#endif
