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

#include <DOM/DOMDocument.h>

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
		unsigned int running_tasks,queued_tasks,retrying_tasks,error_tasks,waiting_conditions;
		
		unsigned int workflow_schedule_id;
		
		std::vector<unsigned int> notifications;
		
		std::vector<DOMElement> waiting_nodes;
		
		bool is_cancelling;
		
		DOMDocument *xmldoc;
		DOMElement context_job, context_parent_job;
		
		const std::string &logs_directory;
		
		int log_dom_maxsize;
		
		bool saveparameters;
		
		int savepoint_level;
		bool savepoint_retry;
		unsigned int savepoint_retry_times;
		unsigned int savepoint_retry_wait;
		
		bool is_shutting_down;
		
		std::recursive_mutex lock;
	
	public:
		WorkflowInstance(const std::string &workflow_name,WorkflowParameters *parameters, unsigned int workflow_schedule_id = 0,const std::string &workflow_host="", const std::string &workflow_user="", const std::string &workflow_comment="");
		WorkflowInstance(unsigned int workflow_instance_id);
		~WorkflowInstance();
		
		unsigned int GetInstanceID() { return workflow_instance_id; }
		unsigned int GetWorkflowID() { return workflow_id; }
		unsigned int GetErrors() { return error_tasks; }
		const DOMDocument *GetDOM() { return xmldoc; }
		
		// start_stop.cpp
		void Start(bool *workflow_terminated);
		void Resume(bool *workflow_terminated);
		void DebugResume(bool *workflow_terminated);
		void Migrate(bool *workflow_terminated);
		void Cancel();
		void Shutdown();
		
		// task_job.cpp
		void TaskRestart(DOMElement task, bool *workflow_terminated);
		bool TaskStop(DOMElement task,int retval,const char *stdout_output,const char * stderr_output,const char *log_output,bool *workflow_terminated);
		pid_t TaskExecute(DOMElement task,pid_t tid,bool *workflow_terminated);
		void TaskUpdateProgression(DOMElement task, int prct);
		bool KillTask(pid_t pid);
		
		// savepoint.cpp
		void RecordSavepoint();
		
		// WorkflowInstance.cpp
		void SendStatus(QueryResponse *response, bool full_status);
		
	private:
		// WorkflowInstance.cpp
		WorkflowInstance();
		
		// condition_loop
		bool handle_condition(DOMElement node,DOMElement context_node,bool can_wait=true);
		bool handle_loop(DOMElement node,DOMElement context_node,std::vector<DOMElement> &nodes, std::vector<DOMElement> &contexts);
		
		// job_tasks
		void run_tasks(DOMElement job,DOMElement context_node);
		bool run_task(DOMElement task,DOMElement context_node);
		void register_job_functions(DOMElement node);
		void run_subjobs(DOMElement job);
		bool run_subjob(DOMElement subjob,DOMElement context_node);
		void enqueue_task(DOMElement task);
		
		// retry
		void retry_task(DOMElement task);
		void schedule_update(DOMElement task,const std::string &schedule_name,int *retry_delay,int *retry_times);
		
		// savepoint
		void record_savepoint(bool force=false);
		
		// value
		void replace_values(DOMElement task,DOMElement context_node);
		void replace_value(DOMElement input,DOMElement context_node);
		
		// WorkflowInstance.cpp
		void record_log(DOMElement node, const char *log);
		std::string format_datetime();
		void update_job_statistics(const std::string &name,int delta,DOMElement node);
		void clear_statistics();
		bool workflow_ended(void);
		
		// custom_filters
		void fill_custom_filters();
};

#endif
