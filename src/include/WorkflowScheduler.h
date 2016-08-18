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

#ifndef _WORKFLOWSCHEDULER_H_
#define _WORKFLOWSCHEDULER_H_

#include <Scheduler.h>

#include <string>

class WorkflowSchedule;
class WorkflowParameters;
class SocketQuerySAX2Handler;
class QueryResponse;

class WorkflowScheduler:public Scheduler
{
	private:
		struct ScheduledWorkflow:Event
		{
			WorkflowSchedule *workflow_schedule = 0;
			
			virtual ~ScheduledWorkflow();
		};
		
		static WorkflowScheduler *instance;
		
		unsigned int *wfs_ids;
		unsigned int *wfs_wi_ids;
		WorkflowSchedule **wfs_executing_instances;
		int num_wfs;
		
		pthread_mutex_t wfs_mutex;
		
	public:
		WorkflowScheduler();
		virtual ~WorkflowScheduler();
		
		static WorkflowScheduler *GetInstance() { return instance; }
		
		void ScheduleWorkflow(WorkflowSchedule *workflow_schedule, unsigned int workflow_instance_id = 0);
		void ScheduledWorkflowInstanceStop(unsigned int workflow_schedule_id, bool success);
		
		void Reload();
		
		void SendStatus(QueryResponse *response);
		
	protected:
		void event_removed(Event *e, event_reasons reason);
	
	private:
	  void init();
	  
	  int lookup(unsigned int *ids, int num_ids,unsigned int id);
	  int lookup_wfs(unsigned int id);
};

#endif
