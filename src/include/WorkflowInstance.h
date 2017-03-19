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

#include <DOMDocument.h>

#include <string>
#include <vector>
#include <mutex>

class WorkflowParameters;
class WorkflowSchedule;
class QueryResponse;

class WorkflowInstance
{
	private:
		unsigned int workflow_id;
		unsigned int workflow_instance_id;
		unsigned int running_tasks,queued_tasks,retrying_tasks,error_tasks;
		
		unsigned int workflow_schedule_id;
		
		std::vector<unsigned int> notifications;
		
		bool is_cancelling;
		
		DOMDocument *xmldoc;
		
		const std::string &logs_directory;
		
		bool errlogs;
		const std::string &errlogs_directory;
		
		const std::string &tasks_directory;
		const std::string &monitor_path;
		
		bool saveparameters;
		
		int savepoint_level;
		bool savepoint_retry;
		unsigned int savepoint_retry_times;
		unsigned int savepoint_retry_wait;
		
		bool is_shutting_down;
		
		std::recursive_mutex lock;
	
	public:
		WorkflowInstance(const std::string &workflow_name,WorkflowParameters *parameters, unsigned int workflow_schedule_id = 0,const std::string &workflow_host=0, const std::string &workflow_user=0);
		WorkflowInstance(unsigned int workflow_instance_id);
		~WorkflowInstance();
		
		unsigned int GetInstanceID() { return workflow_instance_id; }
		unsigned int GetWorkflowID() { return workflow_id; }
		unsigned int GetErrors() { return error_tasks; }
		
		void Start(bool *workflow_terminated);
		void Resume(bool *workflow_terminated);
		void Migrate(bool *workflow_terminated);
		void Cancel();
		void Shutdown();
		
		void TaskRestart(DOMElement task, bool *workflow_terminated);
		bool TaskStop(DOMElement task,int retval,const char *stdout_output,const char * stderr_output,const char *log_output,bool *workflow_terminated);
		pid_t TaskExecute(DOMElement task,pid_t tid,bool *workflow_terminated);
		void TaskUpdateProgression(DOMElement task, int prct);
		
		void SendStatus(QueryResponse *response, bool full_status);
		void RecordSavepoint();
		
		bool KillTask(pid_t pid);
		
	private:
		WorkflowInstance();
		
		void run(DOMElement job,DOMElement context_node);
		void run_subjobs(DOMElement context_node);
		void enqueue_task(DOMElement task);
		void retry_task(DOMElement task);
		void schedule_update(DOMElement task,const std::string &schedule_name,int *retry_delay,int *retry_times);
		bool workflow_ended(void);
		
		void record_savepoint(bool force=false);
		void replace_value(DOMElement task,DOMElement context_node);
		std::string format_datetime();
		int open_log_file(int tid, int fileno);
		void update_statistics();
		
		static Token *evqGetWorkflowParameter(XPathEval::func_context context,const std::vector<Token *> &args);
};

#endif
