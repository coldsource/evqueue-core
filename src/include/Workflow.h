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

#ifndef _WORKFLOW_H_
#define _WORKFLOW_H_

#include <vector>
#include <string>

class DB;
class SocketQuerySAX2Handler;
class QueryResponse;
class WorkflowParameters;

class Workflow
{
	unsigned int workflow_id;
	std::string workflow_name;
	std::string workflow_xml;
	std::string group;
	std::string comment;
	bool bound_schedule, bound_task;
	std::string lastcommit;
	
	std::vector<unsigned int> notifications;
		
	public:
		Workflow();
		Workflow(DB *db,const std::string &workflow_name);
		
		unsigned int GetID() const { return workflow_id; }
		const std::string GetName() const { return workflow_name; }
		const std::string GetXML() const { return workflow_xml; }
		const std::string GetGroup() const { return group; }
		const std::string GetComment() const { return comment; }
		bool GetIsBoundSchedule() const { return bound_schedule; }
		bool GetIsBoundTask() const { return bound_task; }
		std::vector<unsigned int> GetNotifications() const { return notifications; }
		
		void CheckInputParameters(WorkflowParameters *parameters);
		
		std::string GetLastCommit() const { return lastcommit; }
		void SetLastCommit(const std::string &commit_id);
		
		static bool CheckWorkflowName(const std::string &workflow_name);
		static void Get(unsigned int id, QueryResponse *response);
		static unsigned int Create(const std::string &name, const std::string &base64, const std::string &group, const std::string &comment, const std::string &lastcommit = "");
		static void Edit(unsigned int id, const std::string &name, const std::string &base64, const std::string &group, const std::string &comment);
		static void Delete(unsigned int id, bool *task_deleted);
		
		static void SubscribeNotification(unsigned int id, unsigned int notification_id);
		static void UnsubscribeNotification(unsigned int id, unsigned int notification_id);
		static void ClearNotifications(unsigned int id);
		
		static bool HandleQuery(SocketQuerySAX2Handler *saxh, QueryResponse *response);
		
		static std::string CreateSimpleWorkflow(const std::string &task_name, const std::vector<std::string> &inputs);
	
	private:
		static std::string create_edit_check(const std::string &name, const std::string &base64, const std::string &group, const std::string &comment);
};

#endif
