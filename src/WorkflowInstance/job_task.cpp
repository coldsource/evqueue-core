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

#include <WorkflowInstance/WorkflowInstance.h>
#include <Exception/Exception.h>
#include <WorkflowInstance/ExceptionWorkflowContext.h>
#include <XPath/WorkflowXPathFunctions.h>
#include <Logger/Logger.h>
#include <WorkflowInstance/Task.h>
#include <Process/ProcessManager.h>
#include <Queue/QueuePool.h>
#include <WS/Events.h>

#include <signal.h>

#include <memory>

using namespace std;

void WorkflowInstance::TaskRestart(DOMElement task, bool *workflow_terminated)
{
	unique_lock<recursive_mutex> llock(lock);

	enqueue_task(task);
	retrying_tasks--;
	update_job_statistics("retrying_tasks",-1,task);

	*workflow_terminated = workflow_ended();

	record_savepoint();
}

bool WorkflowInstance::TaskStop(DOMElement task_node,int retval,const char *stdout_output,const char *stderr_output,const char *log_output,bool *workflow_terminated)
{
	unique_lock<recursive_mutex> llock(lock);
	
	Logger::Log(LOG_INFO,"[WID %d] Task %s stopped",workflow_instance_id,task_node.getAttribute("name").c_str());

	task_node.setAttribute("status","TERMINATED");
	task_node.setAttribute("retval",to_string(retval));
	task_node.removeAttribute("tid");
	task_node.removeAttribute("pid");
	if(task_node.hasAttribute("retry_at"))
		task_node.removeAttribute("retry_at");

	TaskUpdateProgression(task_node,100);
	
	// Generate output node
	DOMElement output_element = xmldoc->createElement("output");
	output_element.setAttribute("retval",to_string(retval));
	output_element.setAttribute("execution_time",task_node.getAttribute("execution_time"));
	output_element.setAttribute("exit_time",format_datetime());

	if(retval==0)
	{
		Task task(xmldoc, task_node);

		if(task.GetOutputMethod()==task_output_method::XML)
		{
			unique_ptr<DOMDocument> output_xmldoc(DOMDocument::Parse(stdout_output));

			if(output_xmldoc)
			{
				// XML is valid
				output_element.setAttribute("method","xml");
				output_element.appendChild(xmldoc->importNode(output_xmldoc->getDocumentElement(),true));
				task_node.appendChild(output_element);
			}
			else
			{
				// Task returned successfully but XML is invalid. Unable to continue as following tasks might need XML output
				task_node.setAttribute("status","ABORTED");
				task_node.setAttribute("error","Invalid XML returned");
				
				// Treat output as text
				output_element.setAttribute("method","text");
				task_node.appendChild(output_element);
				record_log(output_element,stdout_output);

				error_tasks++;
				update_job_statistics("error_tasks",1,task_node);
			}
		}
		else if(task.GetOutputMethod()==task_output_method::TEXT)
		{
			// We are in text mode
			output_element.setAttribute("method","text");
			task_node.appendChild(output_element);
			record_log(output_element,stdout_output);
		}
	}
	else
	{
		// Store task log in output node. We treat this as TEXT since errors can corrupt XML
		output_element.setAttribute("method","text");
		task_node.appendChild(output_element);
		record_log(output_element,stdout_output);

		if(is_cancelling)
		{
			// We are in cancelling state, we won't execute anything more
			task_node.setAttribute("error","Won't retry because workflow is cancelling");
			error_tasks++;
			update_job_statistics("error_tasks",1,task_node);
		}
		else
			retry_task(task_node);
	}

	// Add stderr output if present
	if(stderr_output)
	{
		DOMElement stderr_element = xmldoc->createElement("stderr");
		task_node.appendChild(stderr_element);
		record_log(stderr_element,stderr_output);
	}

	// Add evqueue log output if present
	if(log_output)
	{
		DOMElement log_element = xmldoc->createElement("log");
		task_node.appendChild(log_element);
		record_log(log_element,log_output);
	}
	
	Events::GetInstance()->Create("TASK_TERMINATE",workflow_instance_id);

	int tasks_count,tasks_successful;

	{
		unique_ptr<DOMXPathResult> res(xmldoc->evaluate("count(../task)",task_node,DOMXPathResult::FIRST_RESULT_TYPE));
		tasks_count = res->getIntegerValue();
	}

	{
		unique_ptr<DOMXPathResult> res(xmldoc->evaluate("count(../task[(@status='TERMINATED' and @retval = 0) or (@status='SKIPPED')])",task_node,DOMXPathResult::FIRST_RESULT_TYPE));
		tasks_successful = res->getIntegerValue();
	}
	
	running_tasks--;
	update_job_statistics("running_tasks",-1,task_node);

	if(tasks_count==tasks_successful)
	{
		DOMNode job = task_node.getParentNode().getParentNode();
		try
		{
			run_subjobs(job);
		}
		catch(Exception &e)
		{
			// Nothing to do on error, just let the workflow end itself since tasks might still be running
		}
	}
	
	// Reevaluate waiting conditions as state might have changed
	vector<DOMElement> waiting_nodes_copy = waiting_nodes;
	
	// Clear waiting conditions. They will be re-inserted if they still evaluate to false
	waiting_nodes.clear();
	waiting_conditions = 0;
	for(int i=0;i<waiting_nodes_copy.size();i++)
	{
		// Remove previous status and details as condition will be re-evaluated
		waiting_nodes_copy.at(i).removeAttribute("status");
		waiting_nodes_copy.at(i).removeAttribute("details");
		update_job_statistics("waiting_conditions",-1,waiting_nodes_copy.at(i));
		
		if(waiting_nodes_copy.at(i).hasAttribute("context-id"))
		{
			DOMElement context_node = xmldoc->getNodeFromEvqID(waiting_nodes_copy.at(i).getAttribute("context-id"));
		
			// Re-evaluate conditions : will wait till next event or start tasks/jobs if condition evaluates to true
			if(waiting_nodes_copy.at(i).getNodeName()=="task")
			{
				register_job_functions(waiting_nodes_copy.at(i));
				run_task(waiting_nodes_copy.at(i),context_node);
			}
			else if(waiting_nodes_copy.at(i).getNodeName()=="job")
				run_subjob(waiting_nodes_copy.at(i),context_node);
		}
	}

	*workflow_terminated = workflow_ended();

	record_savepoint();

	return true;
}

pid_t WorkflowInstance::TaskExecute(DOMElement task_node,pid_t tid,bool *workflow_terminated)
{
	char buf[32],tid_str[16];
	char *task_name_c;
	int parameters_pipe[2];

	unique_lock<recursive_mutex> llock(lock);

	// As we arrive here, the task is queued. Whatever comes, its status will not be queued anymore (will be executing or aborted)
	queued_tasks--;
	update_job_statistics("queued_tasks",-1,task_node);
	
	try
	{
		ExceptionWorkflowContext ctx(task_node,"Task execution");
		
		if(is_cancelling)
			throw Exception("WorkflowInstance","Aborted on user request");
		
		// Get task informations
		Task task(xmldoc, task_node);
		
		Logger::Log(LOG_INFO,"[WID %d] Executing %s",workflow_instance_id,task.GetPath().c_str());
		
		// Execute task
		pid_t pid = ProcessManager::ExecuteTask(task,tid);
		
		// Update task node
		task_node.setAttribute("status","EXECUTING");
		task_node.setAttribute("pid",to_string(pid));
		task_node.setAttribute("tid",to_string(tid));
		task_node.setAttribute("execution_time",format_datetime());
		
		Events::GetInstance()->Create("TASK_EXECUTE",workflow_instance_id);
		
		*workflow_terminated = workflow_ended();

		record_savepoint();

		return pid;
	}
	catch(Exception &e)
	{
		error_tasks++;
		update_job_statistics("error_tasks",1,task_node);
		
		running_tasks--;
		update_job_statistics("running_tasks",-1,task_node);

		*workflow_terminated = workflow_ended();

		record_savepoint();
		
		Logger::Log(LOG_WARNING,"[WID %d] '%s'",workflow_instance_id,e.error.c_str());

		return -1;
	}
}

void WorkflowInstance::TaskUpdateProgression(DOMElement task, int prct)
{
	unique_lock<recursive_mutex> llock(lock);
	
	task.setAttribute("progression",to_string(prct));
	
	Events::GetInstance()->Create("TASK_PROGRESS", workflow_instance_id);
}

void WorkflowInstance::register_job_functions(DOMElement node)
{
	if(node.getNodeName()=="task")
		context_job = node.getParentNode().getParentNode();
	else
		context_job = node;
	
	xmldoc->getXPath()->RegisterFunction("evqGetCurrentJob",{WorkflowXPathFunctions::evqGetCurrentJob,&context_job});
	
	context_parent_job = context_job.getParentNode().getParentNode();
	xmldoc->getXPath()->RegisterFunction("evqGetParentJob",{WorkflowXPathFunctions::evqGetParentJob,&context_parent_job});
	xmldoc->getXPath()->RegisterFunction("evqGetOutput",{WorkflowXPathFunctions::evqGetOutput,&context_parent_job});
	xmldoc->getXPath()->RegisterFunction("evqGetInput",{WorkflowXPathFunctions::evqGetInput,&context_parent_job});
	xmldoc->getXPath()->RegisterFunction("evqGetContext",{WorkflowXPathFunctions::evqGetContext,&context_parent_job});
}

bool WorkflowInstance::KillTask(pid_t pid)
{
	bool found = false;

	unique_lock<recursive_mutex> llock(lock);

	// Look for EXECUTING tasks with specified PID
	unique_ptr<DOMXPathResult> tasks(xmldoc->evaluate("//task[@status='EXECUTING' and @pid='"+to_string(pid)+"']",xmldoc->getDocumentElement(),DOMXPathResult::SNAPSHOT_RESULT_TYPE));

	int tasks_index = 0;
	while(tasks->snapshotItem(tasks_index++))
	{
		DOMNode task = tasks->getNodeValue();

		kill(pid,SIGTERM);
		found = true;
	}

	return found;
}

void WorkflowInstance::run_subjobs(DOMElement job)
{
	// Loop on subjobs
	ExceptionWorkflowContext ctx(job,"Corrupted workflow");
	
	unique_ptr<DOMXPathResult> subjobs(xmldoc->evaluate("subjobs/job",job,DOMXPathResult::SNAPSHOT_RESULT_TYPE));
	DOMElement subjob;
	
	try
	{
		int subjobs_index = 0;
		while(subjobs->snapshotItem(subjobs_index++))
		{
			subjob = (DOMElement)subjobs->getNodeValue();
			run_subjob(subjob,job);
		}
	}
	catch(Exception &e)
	{
		// Terminate workflow
		error_tasks++;
		
		if(subjob)
		{
			subjob.setAttribute("status","ABORTED");
			update_job_statistics("error_tasks",1,subjob);
		}
		else
			update_job_statistics("error_tasks",1,job);
		throw;
	}
}

bool WorkflowInstance::run_subjob(DOMElement subjob,DOMElement context_node)
{
	register_job_functions(subjob);
	
	// Set context node ID
	subjob.setAttribute("context-id",xmldoc->getNodeEvqID(context_node));
	
	// Set job ID
	xmldoc->getNodeEvqID(subjob);
	
	if(!handle_condition(subjob,context_node))
		return false;

	vector<DOMElement> jobs;
	vector<DOMElement> contexts;
	handle_loop(subjob,context_node,jobs,contexts);
	
	for(int i=0;i<jobs.size();i++)
	{
		// Set new context node ID (based on loop expanding)
		jobs.at(i).setAttribute("context-id",xmldoc->getNodeEvqID(contexts.at(i)));
		
		// Set new job ID
		if(i>=1)
		{
			jobs.at(i).removeAttribute("evqid");
			xmldoc->getNodeEvqID(jobs.at(i));
		}
		
		DOMElement current_job = jobs.at(i);
		xmldoc->getXPath()->RegisterFunction("evqGetCurrentJob",{WorkflowXPathFunctions::evqGetCurrentJob,&current_job});
		if(!handle_condition(jobs.at(i),contexts.at(i)))
					continue;
		
		run_tasks(jobs.at(i),contexts.at(i));
	}
	
	return true;
}

void WorkflowInstance::run_tasks(DOMElement job,DOMElement context_node)
{
	int jobs_index = 0;

	// Loop for tasks
	ExceptionWorkflowContext ctx(job,"Corrupted workflow");
	unique_ptr<DOMXPathResult> tasks(xmldoc->evaluate("tasks/task",job,DOMXPathResult::SNAPSHOT_RESULT_TYPE));

	DOMElement task;
	int tasks_index = 0;

	while(tasks->snapshotItem(tasks_index++))
	{
		task = (DOMElement)tasks->getNodeValue();
		
		run_task(task,context_node);
	}
	
	unique_ptr<DOMXPathResult> res(xmldoc->evaluate("count(tasks/task[(@status='TERMINATED' and @retval = 0) or (@status='SKIPPED')])",job,DOMXPathResult::FIRST_RESULT_TYPE));
	int tasks_successful = res->getIntegerValue();
	
	unique_ptr<DOMXPathResult> res2(xmldoc->evaluate("count(tasks/task)",job,DOMXPathResult::FIRST_RESULT_TYPE));
	int tasks_total = res2->getIntegerValue();
	
	// If all task of job is skipped goto child job :
	if (tasks_successful == tasks_total){
		run_subjobs(job);
	}
}

bool WorkflowInstance::run_task(DOMElement task,DOMElement context_node)
{
	// Get task status (if present)
	string task_status;
	if(task.hasAttribute("status") && task.getAttribute("status")=="TERMINATED")
		return false; // Skip tasks that are already terminated (can happen on resume)
	
	// Set context node ID
	task.setAttribute("context-id",xmldoc->getNodeEvqID(context_node));
	
	// Set task ID
	xmldoc->getNodeEvqID(task);

	if(!handle_condition(task,context_node))
		return false;
	
	vector<DOMElement> tasks;
	vector<DOMElement> contexts;
	handle_loop(task,context_node,tasks,contexts);
	
	for(int i=0;i<tasks.size();i++)
	{
		// Set new context node ID (based on loop expanding)
		tasks.at(i).setAttribute("context-id",xmldoc->getNodeEvqID(contexts.at(i)));
		
		// Set new task id
		if(i>=1)
		{
			tasks.at(i).removeAttribute("evqid");
			xmldoc->getNodeEvqID(tasks.at(i));
		}
		
		if(!handle_condition(tasks.at(i),contexts.at(i)))
			continue;
		
		replace_values(tasks.at(i),contexts.at(i));
		enqueue_task(tasks.at(i));
	}

	return true;
}

void WorkflowInstance::enqueue_task(DOMElement task)
{
	if(is_cancelling)
	{
		// Workflow is in chancelling state, we won't queue anything more
		task.setAttribute("status","ABORTED");
		task.setAttribute("error","Aborted on user request");

		error_tasks++;
		update_job_statistics("error_tasks",1,task);

		return;
	}

	// Get task name
	string task_name = task.getAttribute("name");

	// Get queue name (if present) and update task node
	string queue_name = task.getAttribute("queue");
	
	// Compute dynamic queue host
	string queue_host;
	if(task.hasAttribute("queue_host"))
		queue_host = task.getAttribute("queue_host");
	else if(task.hasAttribute("host"))
		queue_host = task.getAttribute("host");

	task.setAttribute("status","QUEUED");

	QueuePool *pool = QueuePool::GetInstance();

	if(!pool->EnqueueTask(queue_name,queue_host,this,task))
	{
		task.setAttribute("status","ABORTED");
		task.setAttribute("error","Unknown queue name");

		error_tasks++;
		update_job_statistics("error_tasks",1,task);

		Logger::Log(LOG_WARNING,"[WID %d] Unknown queue name '%s'",workflow_instance_id,queue_name.c_str());
		return;
	}

	running_tasks++;
	update_job_statistics("running_tasks",1,task);
	
	queued_tasks++;
	update_job_statistics("queued_tasks",1,task);

	Logger::Log(LOG_INFO,"[WID %d] Added task %s to queue %s\n",workflow_instance_id,task_name.c_str(),queue_name.c_str());
	
	Events::GetInstance()->Create("TASK_ENQUEUE",workflow_instance_id);
	
	return;
}
