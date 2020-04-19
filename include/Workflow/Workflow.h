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
class XMLQuery;
class QueryResponse;
class WorkflowParameters;
class User;
class DOMDocument;

class Workflow
{
	unsigned int workflow_id;
	std::string workflow_name;
	std::string workflow_xml;
	std::string group;
	std::string comment;
	bool bound_schedule;
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
		std::vector<unsigned int> GetNotifications() const { return notifications; }
		
		void CheckInputParameters(WorkflowParameters *parameters);
		
		std::string GetLastCommit() const { return lastcommit; }
		bool GetIsModified();
		void SetLastCommit(const std::string &commit_id);
		
		std::string SaveToXML();
		static void LoadFromXML(std::string name, DOMDocument *xmldoc, std::string repo_lastcommit);
		
		static bool CheckWorkflowName(const std::string &workflow_name);
		static void Get(unsigned int id, QueryResponse *response);
		static void Get(const std::string &name, QueryResponse *response);
		static unsigned int Create(const std::string &name, const std::string &base64, const std::string &group, const std::string &comment, const std::string &lastcommit = "");
		static void Edit(unsigned int id, const std::string &name, const std::string &base64, const std::string &group, const std::string &comment);
		static void Delete(unsigned int id);
		
		static void SubscribeNotification(unsigned int id, unsigned int notification_id);
		static void UnsubscribeNotification(unsigned int id, unsigned int notification_id);
		static void ClearNotifications(unsigned int id);
		static void ListNotifications(unsigned int id, QueryResponse *response);
		
		static bool HandleQuery(const User &user, XMLQuery *query, QueryResponse *response);
		
		static void ValidateXML(const std::string &xml_str);
	
	private:
		static std::string create_edit_check(const std::string &name, const std::string &base64, const std::string &group, const std::string &comment);
		static void get(const Workflow &workflow, QueryResponse *response);
};

#endif
