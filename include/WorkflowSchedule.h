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

#ifndef _WORKFLOWSCHEDULE_H_
#define _WORKFLOWSCHEDULE_H_

#include <WorkflowParameters.h>
#include <Schedule.h>

#include <time.h>

enum onfailure_behavior {CONTINUE, SUSPEND};

class WorkflowSchedule
{
	private:
		unsigned int workflow_schedule_id;
		char * workflow_name;
		onfailure_behavior onfailure;
		char *workflow_schedule_host;
		char *workflow_schedule_user;
		
		WorkflowParameters parameters;
		
		Schedule *schedule;

	public:
		WorkflowSchedule(unsigned int workflow_schedule_id);
		~WorkflowSchedule();
		
		const char *GetWorkflowName(void) { return workflow_name; }
		unsigned int GetID() { return workflow_schedule_id; }
		unsigned int GetOnFailureBehavior() { return onfailure; }
		const char *GetHost(void) { return workflow_schedule_host; }
		const char *GetUser(void) { return workflow_schedule_user; }
		WorkflowParameters *GetParameters() { return &parameters; }
		time_t GetNextTime(void) { return schedule->GetNextTime(); }
		
		void SetStatus(bool active);
};

#endif
