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

#ifndef _QUEUEPOOL_H_
#define _QUEUEPOOL_H_

#include <xercesc/dom/DOM.hpp>
using namespace xercesc;

#include <sys/types.h>
#include <pthread.h>

#include <map>
#include <string>

class Queue;
class WorkflowInstance;
class QueryResponse;

class QueuePool
{
	private:
		static QueuePool *instance;
		
		int default_scheduler;
		std::map<std::string,Queue *> queues;
		
		Queue **tid_queue;
		DOMNode **tid_task;
		WorkflowInstance **tid_workflow_instance;
		unsigned int maxpid;
		unsigned int lasttid;
		
		pthread_mutex_t mutex;
		pthread_cond_t fork_lock;
		bool fork_locked,fork_possible;
		bool is_shutting_down;
	
	public:
		QueuePool(void);
		~QueuePool(void);
		
		static QueuePool *GetInstance() { return instance; }
		
		Queue *GetQueue(const std::string &name);
		
		bool EnqueueTask(const std::string &queue_name,WorkflowInstance *workflow_instance,DOMNode *task);
		bool DequeueTask(std::string &queue_name,WorkflowInstance **p_workflow_instance,DOMNode **p_task);
		pid_t ExecuteTask(WorkflowInstance *workflow_instance,DOMNode *task,const std::string &queue_name,pid_t task_id);
		bool TerminateTask(pid_t task_id,WorkflowInstance **p_workflow_instance,DOMNode **p_task);
		bool GetTask(pid_t task_id,WorkflowInstance **p_workflow_instance,DOMNode **p_task);
		
		bool CancelTasks(unsigned int workflow_instance_id);
		
		inline unsigned int GetPoolSize(void) { return queues.size(); }
		
		void Reload(void);
		
		bool IsLocked(void);
		
		void Shutdown(void);
		
		void SendStatistics(QueryResponse *response);
	
		int get_scheduler_from_string(std::string scheduler_str);
		static std::string get_scheduler_from_int(int scheduler);
};

#endif
