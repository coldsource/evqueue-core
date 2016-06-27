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

#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <xercesc/dom/DOM.hpp>
using namespace xercesc;

#include <global.h>

#include <deque>
#include <queue>
#include <map>

#define QUEUE_SCHEDULER_FIFO 1
#define QUEUE_SCHEDULER_PRIO 2

class WorkflowInstance;

class Queue
{
	private:
		struct Task
		{
			WorkflowInstance *workflow_instance;
			DOMNode *task;
			Task *next_task;
		};
		
		char name[QUEUE_NAME_MAX_LEN+1];
		unsigned int id;
		
		unsigned int concurrency;
		unsigned int size;
		unsigned int running_tasks;
		
		std::multimap<unsigned int,Task *> prio_queue;
		std::deque<Task *> queue;
		
		std::queue<Task *>cancelled_tasks;
		
		int scheduler;
		
	public:
		Queue(const char *name, int scheduler);
		~Queue();
		
		bool CheckQueueName(const char *queue_name);
		
		void EnqueueTask(WorkflowInstance *workflow_instance,DOMNode *task);
		bool DequeueTask(WorkflowInstance **p_workflow_instance,DOMNode **p_task);
		bool ExecuteTask(void);
		bool TerminateTask(void);
		bool CancelTasks(unsigned int workflow_instance_id);
		
		inline unsigned int GetSize(void) { return size; }
		inline unsigned int GetRunningTasks(void) { return running_tasks; }
		inline unsigned int GetConcurrency(void) { return concurrency; }
		
		bool IsLocked(void);
	
	private:
		void enqueue_task(WorkflowInstance *workflow_instance,DOMNode *task);
		void dequeue_task(WorkflowInstance **p_workflow_instance,DOMNode **p_task);
};

#endif
