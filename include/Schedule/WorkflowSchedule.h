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

#include <Workflow/WorkflowParameters.h>
#include <Schedule/Schedule.h>

#include <time.h>

#include <string>

class XMLQuery;
class QueryResponse;
class User;

enum onfailure_behavior {CONTINUE, SUSPEND};

class WorkflowSchedule
{
	private:
		unsigned int workflow_schedule_id;
		unsigned int workflow_id;
		std::string schedule_description;
		onfailure_behavior onfailure;
		std::string workflow_schedule_host;
		std::string workflow_schedule_user;
		bool active;
		std::string comment;
		std::string node;
		
		WorkflowParameters parameters;
		
		Schedule schedule;

	public:
		WorkflowSchedule() {}
		WorkflowSchedule(unsigned int workflow_schedule_id);
		~WorkflowSchedule();
		
		unsigned int GetID() { return workflow_schedule_id; }
		const std::string GetWorkflowName() const;
		unsigned int GetWorkflowID() { return workflow_id; }
		const std::string &GetScheduleDescription() { return schedule_description; }
		unsigned int GetOnFailureBehavior() { return onfailure; }
		const std::string &GetHost() { return workflow_schedule_host; }
		const std::string &GetUser() { return workflow_schedule_user; }
		bool GetIsActive() { return active; }
		const std::string &GetComment() { return comment; }
		const std::string &GetNode() { return node; }
		WorkflowParameters *GetParameters() { return &parameters; }
		time_t GetNextTime() { return schedule.GetNextTime(); }
		
		static void Get(unsigned int id, QueryResponse *response);
		
		static unsigned int Create(
			unsigned int workflow_id,
			const std::string &node,
			const std::string &schedule_description,
			bool onfailure_continue,
			const std::string &user,
			const std::string &host,
			bool active,
			const std::string &comment,
			WorkflowParameters *parameters
		);
		
		static void Edit(
			unsigned int id,
			unsigned int workflow_id,
			const std::string &node,
			const std::string &schedule_description,
			bool onfailure_continue,
			const std::string &user,
			const std::string &host,
			bool active,
			const std::string &comment,
			WorkflowParameters *parameters
		);
		
		static void SetIsActive(unsigned int id,bool active);
		
		static void Delete(unsigned int id);
		
		static bool HandleQuery(const User &user, XMLQuery *query, QueryResponse *response);
		
		void SetStatus(bool active);
	
	private:
		static void create_edit_check(
			unsigned int workflow_id,
			const std::string &node,
			const std::string &schedule_description,
			bool onfailure_continue,
			const std::string &user,
			const std::string &host,
			bool active,
			const std::string &comment,
			WorkflowParameters *parameters
		);
};

#endif
