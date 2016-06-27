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
#include <pthread.h>

class WorkflowInstance;

class WorkflowInstances
{
	static WorkflowInstances *instance;
	
	pthread_mutex_t lock;
	
	std::map<unsigned int,WorkflowInstance *> wi;
	
	std::map<unsigned int,pthread_cond_t *> wi_wait;
	
	public:
		WorkflowInstances();
		
		static WorkflowInstances *GetInstance() { return instance; }
		
		void Add(unsigned int workflow_instance_id, WorkflowInstance *workflow_instance);
		void Remove(unsigned int workflow_instance_id);
		bool Cancel(unsigned int workflow_instance_id);
		bool Wait(unsigned int workflow_instance_id, int timeout=0);
		bool KillTask(unsigned int workflow_instance_id, pid_t pid);
		
		void SendStatus(int s);
		bool SendStatus(int s,unsigned int workflow_instance_id);
		void RecordSavepoint();
	
	private:
		void release_waiters(unsigned int workflow_instance_id);
};

#endif
