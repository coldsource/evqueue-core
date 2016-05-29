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

class Queue;
class WorkflowInstance;

class QueuePool
{
	private:
		static QueuePool *instance;
		
		char **name;
		Queue **queue;
		unsigned int pool_size;
		
		unsigned int *tid_queue_index;
		DOMNode **tid_task;
		WorkflowInstance **tid_workflow_instance;
		unsigned int maxpid;
		unsigned int lasttid;
		
		pthread_mutex_t mutex;
		pthread_cond_t fork_lock;
		bool fork_locked,fork_possible;
		bool is_shutting_down;
		
		int lookup(const char *queue_name);
	
	public:
		QueuePool(void);
		~QueuePool(void);
		
		static QueuePool *GetInstance() { return instance; }
		
		Queue *GetQueue(const char *queue_name);
		
		bool EnqueueTask(const char *queue_name,WorkflowInstance *workflow_instance,DOMNode *task);
		bool DequeueTask(char *queue_name,WorkflowInstance **p_workflow_instance,DOMNode **p_task);
		pid_t ExecuteTask(WorkflowInstance *workflow_instance,DOMNode *task,const char *queue_name,pid_t task_id);
		bool TerminateTask(pid_t task_id,WorkflowInstance **p_workflow_instance,DOMNode **p_task);
		bool GetTask(pid_t task_id,WorkflowInstance **p_workflow_instance,DOMNode **p_task);
		
		bool CancelTasks(unsigned int workflow_instance_id);
		
		inline unsigned int GetPoolSize(void) { return pool_size; }
		
		bool IsLocked(void);
		
		void Shutdown(void);
		
		void SendStatistics(int s);
};

#endif
