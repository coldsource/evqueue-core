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

#ifndef _WORKFLOWINSTANCES_H_
#define _WORKFLOWINSTANCES_H_

#include <map>
#include <mutex>
#include <condition_variable>

class WorkflowInstance;
class QueryResponse;
class XMLQuery;
class User;

class WorkflowInstances
{
	static WorkflowInstances *instance;
	
	int poll_interval;
	
	bool is_shutting_down;
	
	std::mutex lock;
	
	std::map<unsigned int,WorkflowInstance *> wi;
	std::map<unsigned int,std::condition_variable *> wi_wait;
	
	public:
		WorkflowInstances();
		
		static WorkflowInstances *GetInstance() { return instance; }
		
		void Add(unsigned int workflow_instance_id, WorkflowInstance *workflow_instance);
		void Remove(unsigned int workflow_instance_id);
		bool Cancel(const User &user, unsigned int workflow_instance_id);
		bool Wait(const User &user, QueryResponse *response, unsigned int workflow_instance_id, int timeout=0);
		bool KillTask(const User &user, unsigned int workflow_instance_id, pid_t pid);
		
		void SendStatus(const User &user, QueryResponse *response, int limit = 0);
		bool SendInstanceStatus(const User &user, QueryResponse *response,unsigned int workflow_instance_id);
		void RecordSavepoint();
		
		static bool HandleQuery(const User &user, XMLQuery *query, QueryResponse *response);
	
	private:
		void release_waiters(unsigned int workflow_instance_id);
};

#endif
