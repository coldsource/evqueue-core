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

#ifndef _WORKFLOWINSTANCE_H_
#define _WORKFLOWINSTANCE_H_

#include <xercesc/dom/DOM.hpp>
#include <pthread.h>

using namespace xercesc;

class WorkflowParameters;
class WorkflowSchedule;

class WorkflowInstance
{
	private:
		DOMLSParser *parser;
		DOMLSSerializer *serializer;
		DOMXPathNSResolver *resolver;
		
		unsigned int workflow_instance_id;
		unsigned int running_tasks,retrying_tasks,error_tasks;
		
		unsigned int workflow_schedule_id;
		
		bool is_cancelling;
		
		DOMDocument *xmldoc;
		
		const char *logs_directory;
		unsigned int logs_directory_len;
		
		bool errlogs;
		const char *errlogs_directory;
		unsigned int errlogs_directory_len;
		
		const char *tasks_directory;
		unsigned int tasks_directory_len;
		
		const char *monitor_path;
		unsigned int monitor_path_len;
		
		bool saveparameters;
		
		int savepoint_level;
		bool savepoint_retry;
		unsigned int savepoint_retry_times;
		unsigned int savepoint_retry_wait;
		
		pthread_mutex_t lock;
	
	public:
		WorkflowInstance(const char *workflow_name,WorkflowParameters *parameters, unsigned int workflow_schedule_id = 0,const char *workflow_host=0, const char *workflow_user=0);
		WorkflowInstance(unsigned int workflow_instance_id);
		~WorkflowInstance();
		
		unsigned int GetInstanceID() { return workflow_instance_id; }
		
		void Start(bool *workflow_terminated);
		void Resume(bool *workflow_terminated);
		void Cancel();
		
		void TaskRestart(DOMNode *task, bool *workflow_terminated);
		bool TaskStop(DOMNode *task,int retval,const char *output,bool *workflow_terminated);
		pid_t TaskExecute(DOMNode *task,pid_t tid,bool *workflow_terminated);
		bool CheckTaskName(const char *task_name);
		
		void SendStatus(int s);
		void RecordSavepoint();
		
		bool KillTask(pid_t pid);
		
	private:
		void init(void);
		
		void run(DOMNode *job,DOMNode *context_node);
		void run_subjobs(DOMNode *context_node);
		void enqueue_task(DOMNode *task);
		void retry_task(DOMNode *task);
		void schedule_update(DOMNode *task,const char *schedule_name,int *retry_delay,int *retry_times);
		bool workflow_ended(void);
		
		void record_savepoint(bool force=false);
		void replace_value(DOMNode *task,DOMNode *context_node);
		void format_datetime(char *str);
};

#endif
