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

#include <WorkflowInstance.h>
#include <WorkflowParameters.h>
#include <WorkflowSchedule.h>
#include <WorkflowScheduler.h>
#include <Workflows.h>
#include <Workflow.h>
#include <WorkflowXPathFunctions.h>
#include <QueuePool.h>
#include <DB.h>
#include <Exception.h>
#include <ExceptionWorkflowContext.h>
#include <Retrier.h>
#include <Statistics.h>
#include <Configuration.h>
#include <WorkflowInstances.h>
#include <Logger.h>
#include <Tasks.h>
#include <Task.h>
#include <RetrySchedules.h>
#include <RetrySchedule.h>
#include <SequenceGenerator.h>
#include <Notifications.h>
#include <Sockets.h>
#include <QueryResponse.h>
#include <XMLUtils.h>
#include <ProcessManager.h>
#include <tools.h>
#include <global.h>

#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <memory>

using namespace std;

WorkflowInstance::WorkflowInstance(void):
	logs_directory(Configuration::GetInstance()->Get("processmanager.logs.directory")),
	errlogs_directory(Configuration::GetInstance()->Get("processmanager.errlogs.directory"))
{
	workflow_instance_id = 0;
	workflow_schedule_id = 0;

	errlogs = Configuration::GetInstance()->GetBool("processmanager.errlogs.enable");

	saveparameters = Configuration::GetInstance()->GetBool("workflowinstance.saveparameters");

	savepoint_level = Configuration::GetInstance()->GetInt("workflowinstance.savepoint.level");
	if(savepoint_level<0 || savepoint_level>3)
		savepoint_level = 0;

	savepoint_retry = Configuration::GetInstance()->GetBool("workflowinstance.savepoint.retry.enable");

	if(savepoint_retry)
	{
		savepoint_retry_times = Configuration::GetInstance()->GetInt("workflowinstance.savepoint.retry.times");
		savepoint_retry_wait = Configuration::GetInstance()->GetInt("workflowinstance.savepoint.retry.wait");
	}
	
	xmldoc = 0;

	is_cancelling = false;

	running_tasks = 0;
	queued_tasks = 0;
	retrying_tasks = 0;
	error_tasks = 0;

	is_shutting_down = false;
}

WorkflowInstance::WorkflowInstance(const string &workflow_name,WorkflowParameters *parameters, unsigned int workflow_schedule_id,const string &workflow_host, const string &workflow_user):
	WorkflowInstance()
{
	DB db;

	if(workflow_name.length()>WORKFLOW_NAME_MAX_LEN)
		throw Exception("WorkflowInstance","Workflow name is too long");

	// Get workflow. This will throw an exception if workflow doesn't exist
	Workflow workflow = Workflows::GetInstance()->Get(workflow_name);

	workflow_id = workflow.GetID();
	notifications = workflow.GetNotifications();

	this->workflow_schedule_id = workflow_schedule_id;

	// Load workflow XML
	xmldoc = DOMDocument::Parse(workflow.GetXML());
	xmldoc->getXPath()->RegisterFunction("evqGetWorkflowParameter",{WorkflowXPathFunctions::evqGetWorkflowParameter,0});
	
	// Set workflow name for front-office display
	xmldoc->getDocumentElement().setAttribute("name",workflow_name);

	// Set workflow user and host
	if(workflow_host.length())
	{
		xmldoc->getDocumentElement().setAttribute("host",workflow_host);

		if(workflow_user.length())
			xmldoc->getDocumentElement().setAttribute("user",workflow_user);
	}

	// Set input parameters
	string parameter_name;
	string parameter_value;
	int passed_parameters = 0;

	parameters->SeekStart();
	while(parameters->Get(parameter_name,parameter_value))
	{
		unique_ptr<DOMXPathResult> res(xmldoc->evaluate("parameters/parameter[@name = '"+parameter_name+"']",xmldoc->getDocumentElement(),DOMXPathResult::FIRST_RESULT_TYPE));
		if(res->isNode())
		{
			DOMNode parameter_node = res->getNodeValue();
			parameter_node.setTextContent(parameter_value);
		}
		else
			throw Exception("WorkflowInstance","Unknown parameter : "+parameter_name);

		passed_parameters++;
	}

	{
		unique_ptr<DOMXPathResult> res(xmldoc->evaluate("count(parameters/parameter)",xmldoc->getDocumentElement(),DOMXPathResult::FIRST_RESULT_TYPE));
		int workflow_template_parameters = res->getIntegerValue();

		if(workflow_template_parameters!=passed_parameters)
		{
			char e[256];
			sprintf(e, "Invalid number of parameters passed to workflow (passed %d, expected %d)",passed_parameters,workflow_template_parameters);
			throw Exception("WorkflowInstance",e);
		}
	}
	
	// Import schedule
	{
		unique_ptr<DOMXPathResult> tasks(xmldoc->evaluate("//task[@retry_schedule]",xmldoc->getDocumentElement(),DOMXPathResult::SNAPSHOT_RESULT_TYPE));

		unsigned int tasks_index = 0;
		while(tasks->snapshotItem(tasks_index++))
		{
			DOMNode task = tasks->getNodeValue();

			string schedule_name = ((DOMElement)task).getAttribute("retry_schedule");
			
			unique_ptr<DOMXPathResult> res(xmldoc->evaluate("schedules/schedule[@name = '"+schedule_name+"']",xmldoc->getDocumentElement(),DOMXPathResult::FIRST_RESULT_TYPE));
			if(!res->isNode())
			{
				// Schedule is not local, check global
				RetrySchedule retry_schedule;
				retry_schedule = RetrySchedules::GetInstance()->Get(schedule_name);

				// Import global schedule
				unique_ptr<DOMDocument> schedule_xmldoc(DOMDocument::Parse(retry_schedule.GetXML()));
				schedule_xmldoc->getDocumentElement().setAttribute("name",retry_schedule.GetName());

				// Add schedule to current workflow
				DOMNode schedule_node = xmldoc->importNode(schedule_xmldoc->getDocumentElement(),true);

				unique_ptr<DOMXPathResult> res(xmldoc->evaluate("schedules",xmldoc->getDocumentElement(),DOMXPathResult::FIRST_RESULT_TYPE));
				DOMNode schedules_nodes;
				if(res->isNode())
					schedules_nodes = res->getNodeValue();
				else
				{
					schedules_nodes = xmldoc->createElement("schedules");
					xmldoc->getDocumentElement().appendChild(schedules_nodes);
				}
				
				schedules_nodes.appendChild(schedule_node);
			}
		}
	}

	if(savepoint_level>=2)
	{
		// Insert workflow instance in DB
		db.QueryPrintf("INSERT INTO t_workflow_instance(node_name,workflow_id,workflow_schedule_id,workflow_instance_host,workflow_instance_status,workflow_instance_start) VALUES(%s,%i,%i,%s,'EXECUTING',NOW())",&Configuration::GetInstance()->Get("cluster.node.name"),&workflow_id,workflow_schedule_id?&workflow_schedule_id:0,&workflow_host);
		this->workflow_instance_id = db.InsertID();

		// Save workflow parameters
		if(saveparameters)
		{
			parameters->SeekStart();
			while(parameters->Get(parameter_name,parameter_value))
				db.QueryPrintf("INSERT INTO t_workflow_instance_parameters VALUES(%i,%s,%s)",&this->workflow_instance_id,&parameter_name,&parameter_value);
		}
	}
	else
	{
		this->workflow_instance_id = SequenceGenerator::GetInstance()->GetInc();
	}
	
	// Save workflow
	record_savepoint();

	// Register new instance
	WorkflowInstances::GetInstance()->Add(workflow_instance_id, this);

	Statistics::GetInstance()->IncWorkflowInstanceExecuting();
}

WorkflowInstance::WorkflowInstance(unsigned int workflow_instance_id):
	WorkflowInstance()
{
	DB db;

	// Check workflow exists
	db.QueryPrintf("SELECT workflow_instance_savepoint, workflow_schedule_id, workflow_id FROM t_workflow_instance WHERE workflow_instance_id='%i'",&workflow_instance_id);

	if(!db.FetchRow())
		throw Exception("WorkflowInstance","Unknown workflow instance");

	if(db.GetFieldIsNULL(0) || db.GetField(0).length()==0)
	{
		db.QueryPrintf("UPDATE t_workflow_instance SET workflow_instance_status='TERMINATED' WHERE workflow_instance_id=%i",&workflow_instance_id);

		throw Exception("WorkflowInstance","Could not resume workflow : empty savepoint");
	}

	this->workflow_instance_id = workflow_instance_id;

	// Load workflow XML
	xmldoc = DOMDocument::Parse(db.GetField(0));
	xmldoc->getXPath()->RegisterFunction("evqGetWorkflowParameter",{WorkflowXPathFunctions::evqGetWorkflowParameter,xmldoc->getXPath()});

	// Load workflow schedule if necessary
	workflow_schedule_id = db.GetFieldInt(1);

	workflow_id = db.GetFieldInt(2);

	// Register new instance
	WorkflowInstances::GetInstance()->Add(workflow_instance_id, this);

	Statistics::GetInstance()->IncWorkflowInstanceExecuting();
}

WorkflowInstance::~WorkflowInstance()
{
	if(!is_shutting_down)
	{
		// Call notification scripts before removing instance from active workflows so they can call the engine to get instance XML
		for(int i = 0; i < notifications.size(); i++)
			Notifications::GetInstance()->Call(notifications.at(i),this);

		// Unregister new instance to ensure no one is still using it
		WorkflowInstances::GetInstance()->Remove(workflow_instance_id);

		if(workflow_schedule_id)
		{
			// If this is a scheduled workflow, notify scheduler of end
			WorkflowScheduler *scheduler = WorkflowScheduler::GetInstance();
			scheduler->ScheduledWorkflowInstanceStop(workflow_schedule_id,error_tasks==0);
		}

		if(workflow_instance_id)
			Statistics::GetInstance()->DecWorkflowInstanceExecuting();
	}

	if(xmldoc)
		delete xmldoc;

	if(!is_shutting_down)
		Logger::Log(LOG_INFO,"[WID %d] Terminated",workflow_instance_id);
	else
		Logger::Log(LOG_NOTICE,"[WID %d] Suspended during shutdown",workflow_instance_id);
}

void WorkflowInstance::Start(bool *workflow_terminated)
{
	unique_lock<recursive_mutex> llock(lock);

	Logger::Log(LOG_INFO,"[WID %d] Started",workflow_instance_id);

	xmldoc->getDocumentElement().setAttribute("id",to_string(workflow_instance_id));
	xmldoc->getDocumentElement().setAttribute("status","EXECUTING");
	xmldoc->getDocumentElement().setAttribute("start_time",format_datetime());

	try
	{
		run_subjobs(xmldoc->getDocumentElement());
	}
	catch(Exception &e)
	{
		// Nothing to do on error, just let the workflow end itself since tasks might still be running
	}

	*workflow_terminated = workflow_ended();

	record_savepoint();
}

void WorkflowInstance::Resume(bool *workflow_terminated)
{
	QueuePool *qp = QueuePool::GetInstance();

	unique_lock<recursive_mutex> llock(lock);

	// Look for EXECUTING tasks (they might be still in execution or terminated)
	// Also look for TERMINATED tasks because we might have to retry them
	unique_ptr<DOMXPathResult> tasks(xmldoc->evaluate("//task[@status='QUEUED' or @status='EXECUTING' or @status='TERMINATED']",xmldoc->getDocumentElement(),DOMXPathResult::SNAPSHOT_RESULT_TYPE));
	DOMElement task;

	int tasks_index = 0;
	while(tasks->snapshotItem(tasks_index++))
	{
		task = (DOMElement)tasks->getNodeValue();

		if(task.getAttribute("status")=="QUEUED")
			enqueue_task(task);
		else if(task.getAttribute("status")=="EXECUTING")
		{
			// Restore mapping tables in QueuePool for executing tasks
			string queue_name = task.getAttribute("queue");
			pid_t task_id = XMLUtils::GetAttributeInt(task,"tid");
			qp->ExecuteTask(this,task,queue_name,task_id);

			running_tasks++;
		}
		else if(task.getAttribute("status")=="TERMINATED")
		{
			if(task.getAttribute("retval")!="0")
				retry_task(task);
		}
	}

	*workflow_terminated = workflow_ended();

	if(tasks_index>0)
		record_savepoint(); // XML has been modified (tasks status update from DB)
}

void WorkflowInstance::Migrate(bool *workflow_terminated)
{
	unique_lock<recursive_mutex> llock(lock);

	// For EXECUTING tasks : since we are migrating from another node, they will never end, fake termination
	unique_ptr<DOMXPathResult> tasks(xmldoc->evaluate("//task[@status='QUEUED' or @status='EXECUTING' or @status='TERMINATED']",xmldoc->getDocumentElement(),DOMXPathResult::SNAPSHOT_RESULT_TYPE));
	DOMElement task;

	int tasks_index = 0;
	while(tasks->snapshotItem(tasks_index++))
	{
		task = (DOMElement)tasks->getNodeValue();

		if(task.getAttribute("status")=="QUEUED")
			enqueue_task(task);
		else if(task.getAttribute("status")=="EXECUTING")
		{
			// Fake task ending with generic error code
			running_tasks++; // Inc running_tasks before calling TaskStop
			TaskStop(task,-1,"Task migrated",0,0,workflow_terminated);
		}
		else if(task.getAttribute("status")=="TERMINATED")
		{
			if(task.getAttribute("retval")!="0")
				retry_task(task);
		}
	}

	*workflow_terminated = workflow_ended();

	if(tasks_index>0)
		record_savepoint(); // XML has been modified (tasks status update from DB)

	// Workflow is migrated, update node name
	Configuration *config = Configuration::GetInstance();

	DB db;
	db.QueryPrintf("UPDATE t_workflow_instance SET node_name=%s WHERE workflow_instance_id=%i",&config->Get("cluster.node.name"),&workflow_instance_id);
}

void WorkflowInstance::Cancel()
{
	Logger::Log(LOG_NOTICE,"[WID %d] Cancelling",workflow_instance_id);

	unique_lock<recursive_mutex> llock(lock);

	is_cancelling = true;
}

void WorkflowInstance::Shutdown(void)
{
	is_shutting_down = true;
}

void WorkflowInstance::TaskRestart(DOMElement task, bool *workflow_terminated)
{
	unique_lock<recursive_mutex> llock(lock);

	enqueue_task(task);
	retrying_tasks--;

	*workflow_terminated = workflow_ended();

	record_savepoint();
}

bool WorkflowInstance::TaskStop(DOMElement task_node,int retval,const char *stdout_output,const char *stderr_output,const char *log_output,bool *workflow_terminated)
{
	unique_lock<recursive_mutex> llock(lock);

	task_node.setAttribute("status","TERMINATED");
	task_node.setAttribute("retval",to_string(retval));
	task_node.removeAttribute("tid");
	task_node.removeAttribute("pid");

	TaskUpdateProgression(task_node,100);

	// Generate output node
	DOMElement output_element = xmldoc->createElement("output");
	output_element.setAttribute("retval",to_string(retval));
	output_element.setAttribute("execution_time",task_node.getAttribute("execution_time"));
	output_element.setAttribute("exit_time",format_datetime());

	if(retval==0)
	{
		Task task;

		try
		{
			ExceptionWorkflowContext ctx(task_node,"Task has vanished");
			task = Tasks::GetInstance()->Get(task_node.getAttribute("name"));
		}
		catch(Exception e)
		{
			error_tasks++;
		}

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
				if(errlogs)
				{
					char *errlog_filename = new char[errlogs_directory.length()+32];
					sprintf(errlog_filename,"%s/%d-XXXXXX.log",errlogs_directory.c_str(),workflow_instance_id);

					int fno = mkstemps(errlog_filename,4);
					int stdout_output_len = strlen(stdout_output);
					if(write(fno,stdout_output,stdout_output_len)!=stdout_output_len)
						Logger::Log(LOG_WARNING,"[WID %d] Error writing error log file %s",workflow_instance_id,errlog_filename);
					close(fno);

					Logger::Log(LOG_WARNING,"[WID %d] Invalid XML returned, output has been saved as %s",workflow_instance_id,errlog_filename);

					delete[] errlog_filename;
				}

				task_node.setAttribute("status","ABORTED");
				task_node.setAttribute("error","Invalid XML returned");

				error_tasks++;
			}
		}
		else if(task.GetOutputMethod()==task_output_method::TEXT)
		{
			// We are in text mode
			output_element.setAttribute("method","text");
			output_element.appendChild(xmldoc->createTextNode(stdout_output));
			task_node.appendChild(output_element);
		}
	}
	else
	{
		// Store task log in output node. We treat this as TEXT since errors can corrupt XML
		output_element.setAttribute("method","text");
		output_element.appendChild(xmldoc->createTextNode(stdout_output));
		task_node.appendChild(output_element);

		if(is_cancelling)
		{
			// We are in cancelling state, we won't execute anything more
			task_node.setAttribute("error","Won't retry because workflow is cancelling");
			error_tasks++;
		}
		else
			retry_task(task_node);
	}

	// Add stderr output if present
	if(stderr_output)
	{
		DOMElement stderr_element = xmldoc->createElement("stderr");
		stderr_element.appendChild(xmldoc->createTextNode(stderr_output));
		task_node.appendChild(stderr_element);
	}

	// Add evqueue log output if present
	if(log_output)
	{
		DOMElement log_element = xmldoc->createElement("log");
		log_element.appendChild(xmldoc->createTextNode(log_output));
		task_node.appendChild(log_element);
	}

	int tasks_count,tasks_successful;

	{
		unique_ptr<DOMXPathResult> res(xmldoc->evaluate("count(../task)",task_node,DOMXPathResult::FIRST_RESULT_TYPE));
		tasks_count = res->getIntegerValue();
	}

	{
		unique_ptr<DOMXPathResult> res(xmldoc->evaluate("count(../task[(@status='TERMINATED' and @retval = 0) or (@status='SKIPPED')])",task_node,DOMXPathResult::FIRST_RESULT_TYPE));
		tasks_successful = res->getIntegerValue();
	}

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

	running_tasks--;

	*workflow_terminated = workflow_ended();

	record_savepoint();

	return true;
}

pid_t WorkflowInstance::TaskExecute(DOMElement task_node,pid_t tid,bool *workflow_terminated)
{
	char buf[32],tid_str[16];
	char *task_name_c;
	int parameters_pipe[2];
	Task task;

	unique_lock<recursive_mutex> llock(lock);

	// As we arrive here, the task is queued. Whatever comes, its status will not be queued anymore (will be executing or aborted)
	queued_tasks--;
	
	try
	{
		ExceptionWorkflowContext ctx(task_node,"Task execution");
		
		if(is_cancelling)
			throw Exception("WorkflowInstance","Aborted on user request");
		
		// Get task informations
		string task_name = task_node.getAttribute("name");
		
		// Prepare parameters
		// This must be done before fork() because xerces Transcode() can deadlock on fork (xerces bug...)
		vector<string> parameters_name;
		vector<string> parameters_value;
		{
			unique_ptr<DOMXPathResult> parameters(xmldoc->evaluate("input",task_node,DOMXPathResult::SNAPSHOT_RESULT_TYPE));
			DOMElement parameter;
			int parameters_index = 0;

			while(parameters->snapshotItem(parameters_index))
			{
				parameter = (DOMElement)parameters->getNodeValue();

				if(parameter.hasAttribute("name"))
					parameters_name.push_back(parameter.getAttribute("name"));
				else
					parameters_name.push_back("");

				parameters_value.push_back(parameter.getTextContent());

				parameters_index++;
			}
		}
		
		// Prepare pipe for SDTIN
		string stdin_parameter;
		{
			unique_ptr<DOMXPathResult> parameters(xmldoc->evaluate("stdin",task_node,DOMXPathResult::FIRST_RESULT_TYPE));

			if(parameters->isNode())
			{
				DOMElement stdin_node = (DOMElement)parameters->getNodeValue();
				if(stdin_node.hasAttribute("mode") && stdin_node.getAttribute("mode")=="text")
					stdin_parameter = stdin_node.getTextContent();
				else
					stdin_parameter = xmldoc->Serialize(stdin_node);
			}
		}
		
		// Get user/host informations
		string user;
		string host;
		if(task_node.hasAttribute("host"))
		{
			host = task_node.getAttribute("host");
			if(task_node.hasAttribute("user"))
				user = task_node.getAttribute("user");
		}
		else if(xmldoc->getDocumentElement().hasAttribute("host"))
		{
			host = xmldoc->getDocumentElement().getAttribute("host");
			if(xmldoc->getDocumentElement().hasAttribute("user"))
				user = xmldoc->getDocumentElement().getAttribute("user");
		}
		
		Logger::Log(LOG_INFO,"[WID %d] Executing %s",workflow_instance_id,task_name.c_str());
		
		// Execute task
		pid_t pid = ProcessManager::ExecuteTask(task_name,parameters_name,parameters_value,stdin_parameter,tid,host,user);
		
		// Update task node
		task_node.setAttribute("status","EXECUTING");
		task_node.setAttribute("pid",to_string(pid));
		task_node.setAttribute("tid",to_string(tid));
		task_node.setAttribute("execution_time",format_datetime());
		
		*workflow_terminated = workflow_ended();

		record_savepoint();

		return pid;
	}
	catch(Exception &e)
	{
		error_tasks++;
		running_tasks--;

		*workflow_terminated = workflow_ended();

		record_savepoint();
		
		Logger::Log(LOG_WARNING,"[WID %d] '%s'",workflow_instance_id,e.error.c_str());

		return -1;
	}
}

void WorkflowInstance::TaskUpdateProgression(DOMElement task, int prct)
{
	task.setAttribute("progression",to_string(prct));
}

void WorkflowInstance::SendStatus(QueryResponse *response, bool full_status)
{
	unique_lock<recursive_mutex> llock(lock);

	DOMDocument *status_doc = response->GetDOM();

	if(!full_status)
	{
		DOMNode workflow_node = status_doc->importNode(xmldoc->getDocumentElement(),false);
		status_doc->getDocumentElement().appendChild(workflow_node);
	}
	else
	{
		DOMNode workflow_node = status_doc->importNode(xmldoc->getDocumentElement(),true);
		status_doc->getDocumentElement().appendChild(workflow_node);
	}
}

void WorkflowInstance::RecordSavepoint()
{
	unique_lock<recursive_mutex> llock(lock);

	record_savepoint(true);
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

void WorkflowInstance::retry_task(DOMElement task)
{
	unsigned int task_instance_id;
	int retry_delay,retry_times, schedule_level;
	char buf[32];
	char *retry_schedule_c;

	// Check if we have a retry_schedule
	string retry_schedule ;
	if(task.hasAttribute("retry_schedule"))
		retry_schedule = task.getAttribute("retry_schedule");

	if(task.hasAttribute("retry_schedule_level"))
		schedule_level = XMLUtils::GetAttributeInt(task,"retry_schedule_level");
	else
		schedule_level = 0;

	if(retry_schedule!="" && schedule_level==0)
	{
		// Retry schedule has not yet begun, so init sequence
		schedule_update(task,retry_schedule,&retry_delay,&retry_times);
	}
	else
	{
		// Check retry parameters (retry_delay and retry_times) allow at least one retry
		if(!task.hasAttribute("retry_delay"))
		{
			error_tasks++; // We won't retry this task, set error
			return;
		}

		retry_delay = XMLUtils::GetAttributeInt(task,"retry_delay");

		if(!task.hasAttribute("retry_times"))
		{
			error_tasks++; // We won't retry this task, set error
			return;
		}

		retry_times = XMLUtils::GetAttributeInt(task,"retry_times");
	}

	if(retry_schedule!="" && retry_times==0)
	{
		// We have reached end of the schedule level, update
		schedule_update(task,retry_schedule,&retry_delay,&retry_times);
	}

	if(retry_delay==0 || retry_times==0)
	{
		error_tasks++; // No more retry for this task, set error
		return;
	}

	// If retry_retval is specified, only retry on specified retval
	if(task.hasAttribute("retry_retval") && task.getAttribute("retry_retval")!=task.getAttribute("retval"))
		return;

	// Retry task
	time_t t;
	time(&t);
	t += retry_delay;
	strftime(buf,32,"%Y-%m-%d %H:%M:%S",localtime(&t));
	task.setAttribute("retry_at",buf);

	task.setAttribute("retry_times",to_string(retry_times-1));

	Retrier *retrier = Retrier::GetInstance();
	retrier->InsertTask(this,task,time(0)+retry_delay);

	retrying_tasks++;
}

void WorkflowInstance::run_tasks(DOMElement job,DOMElement context_node)
{
	int jobs_index = 0;

	// Loop for tasks
	ExceptionWorkflowContext ctx(job,"Corrupted workflow");
	unique_ptr<DOMXPathResult> tasks(xmldoc->evaluate("tasks/task",job,DOMXPathResult::SNAPSHOT_RESULT_TYPE));

	DOMElement task;
	int tasks_index = 0;
	int count_tasks_skipped = 0;

	while(tasks->snapshotItem(tasks_index++))
	{
		task = (DOMElement)tasks->getNodeValue();
		
		if(!run_task(task,context_node))
			count_tasks_skipped++;
	}
	// If all task of job is skipped goto child job :
	if (count_tasks_skipped == tasks_index-1){
		run_subjobs(job);
	}
}

bool WorkflowInstance::run_task(DOMElement task,DOMElement context_node)
{
	// Get task status (if present)
	string task_status;
	if(task.hasAttribute("status"))
		task_status = task.getAttribute("status");

	if(task_status=="TERMINATED")
		return false; // Skip tasks that are already terminated (can happen on resume)

	// Check for conditional tasks
	if(task.hasAttribute("condition"))
	{
		ExceptionWorkflowContext ctx(task,"Error evaluating condition");
		
		unique_ptr<DOMXPathResult> test_expr(xmldoc->evaluate(task.getAttribute("condition"),context_node,DOMXPathResult::FIRST_RESULT_TYPE));
		
		if(!test_expr->getIntegerValue())
		{
			task.setAttribute("status","SKIPPED");
			task.setAttribute("details","Condition evaluates to false");
			return false;
		}
	}
	
	// Check for looped tasks (must expand them before execution)
	if(task.hasAttribute("loop"))
	{
		ExceptionWorkflowContext ctx(task,"Error evaluating loop");
		
		string loop_xpath = task.getAttribute("loop");
		task.removeAttribute("loop");
		
		// This is unchecked user input, try evaluation
		DOMNode matching_node;
		unique_ptr<DOMXPathResult> matching_nodes(xmldoc->evaluate(loop_xpath,context_node,DOMXPathResult::SNAPSHOT_RESULT_TYPE));
		
		int matching_nodes_index = 0;
		while(matching_nodes->snapshotItem(matching_nodes_index++))
		{
			matching_node = matching_nodes->getNodeValue();

			DOMNode task_clone = task.cloneNode(true);
			task.getParentNode().appendChild(task_clone);

			// Enqueue task
			replace_value(task_clone,matching_node);
			enqueue_task(task_clone);
		}

		task.getParentNode().removeChild(task);
	}
	else
	{
		// Enqueue task
		replace_value(task,context_node);
		enqueue_task(task);
	}
	
	return true;
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
			xmldoc->getXPath()->RegisterFunction("evqGetParentJob",{WorkflowXPathFunctions::evqGetParentJob,&job});
			xmldoc->getXPath()->RegisterFunction("evqGetOutput",{WorkflowXPathFunctions::evqGetOutput,&job});
			
			subjob = (DOMElement)subjobs->getNodeValue();
		}
	}
	catch(Exception &e)
	{
		// Terminate workflow
		error_tasks++;
		throw e;
	}
}

bool WorkflowInstance::run_subjob(DOMElement subjob)
{
	// Check for conditional jobs
	if(subjob.hasAttribute("condition"))
	{
		ExceptionWorkflowContext ctx(subjob,"Error evaluationg condition");
		
		unique_ptr<DOMXPathResult> test_expr(xmldoc->evaluate(subjob.getAttribute("condition"),subjob.getParentNode(),DOMXPathResult::FIRST_RESULT_TYPE));

		if(!test_expr->getIntegerValue())
		{
			subjob.setAttribute("status","SKIPPED");
			subjob.setAttribute("details","Condition evaluates to false");
			return false;
		}
	}

	// Check for looped jobs (must expand them before execution)
	if(subjob.hasAttribute("loop"))
	{
		ExceptionWorkflowContext ctx(subjob,"Error evaluationg loop");
		
		string loop_xpath = subjob.getAttribute("loop");
		subjob.removeAttribute("loop");

		DOMNode matching_node;
		DOMXPathResult *matching_nodes;

		{
			unique_ptr<DOMXPathResult> matching_nodes(xmldoc->evaluate(loop_xpath,subjob.getParentNode(),DOMXPathResult::SNAPSHOT_RESULT_TYPE));
			
			int matching_nodes_index = 0;
			while(matching_nodes->snapshotItem(matching_nodes_index++))
			{
				matching_node = matching_nodes->getNodeValue();

				DOMNode subjob_clone = subjob.cloneNode(true);
				subjob.getParentNode().appendChild(subjob_clone);

				run_tasks(subjob_clone,matching_node);
			}

			subjob.getParentNode().removeChild(subjob);
		}
	}
	else
		run_tasks(subjob,subjob.getParentNode());
	
	return true;
}

void WorkflowInstance::replace_value(DOMElement task,DOMElement context_node)
{
	DOMElement value;
	int values_index;

	{
		ExceptionWorkflowContext ctx(task,"Error computing input value");
		
		// Replace <value> nodes par their literal value
		unique_ptr<DOMXPathResult> values(xmldoc->evaluate(".//value",task,DOMXPathResult::SNAPSHOT_RESULT_TYPE));
		values_index = 0;
		while(values->snapshotItem(values_index++))
		{
			value = (DOMElement)values->getNodeValue();

			// This is unchecked user input. We have to try evaluation
			unique_ptr<DOMXPathResult> value_nodes(xmldoc->evaluate(value.getAttribute("select"),context_node,DOMXPathResult::FIRST_RESULT_TYPE));
			
			if(value_nodes->isNode())
				value.getParentNode().replaceChild(xmldoc->createTextNode(value_nodes->getStringValue()),value);
		}
	}
	
	// Expand dynamic task host if needed
	if(task.hasAttribute("host"))
	{
		ExceptionWorkflowContext ctx(task,"Error computing dynamic host");
		
		string attr_val = task.getAttribute("host");
		string expanded_attr_val = xmldoc->ExpandXPathAttribute(attr_val,context_node);
		task.setAttribute("host",expanded_attr_val);
	}
	
	// Expand dynamic task user if needed
	if(task.hasAttribute("user"))
	{
		ExceptionWorkflowContext ctx(task,"Error computing dynamic user");
		
		string attr_val = task.getAttribute("user");
		std::string expanded_attr_val = xmldoc->ExpandXPathAttribute(attr_val,context_node);
		task.setAttribute("user",expanded_attr_val);
	}
	
	{
		ExceptionWorkflowContext ctx(task,"Error computing input values from copy node");
		
		// Replace <copy> nodes par their value
		unique_ptr<DOMXPathResult> values(xmldoc->evaluate(".//copy",task,DOMXPathResult::SNAPSHOT_RESULT_TYPE));
		values_index = 0;
		while(values->snapshotItem(values_index++))
		{
			value = (DOMElement)values->getNodeValue();
			string xpath_select = value.getAttribute("select");

			// This is unchecked user input. We have to try evaluation
			unique_ptr<DOMXPathResult> result_nodes(xmldoc->evaluate(xpath_select,context_node,DOMXPathResult::SNAPSHOT_RESULT_TYPE));
			
			int result_index = 0;
			while(result_nodes->snapshotItem(result_index++))
			{
				DOMNode result_node = result_nodes->getNodeValue();
				value.getParentNode().insertBefore(result_node.cloneNode(true),value);
			}

			value.getParentNode().removeChild(value);
		}
	}
}

void WorkflowInstance::enqueue_task(DOMElement task)
{
	if(is_cancelling)
	{
		// Workflow is in chancelling state, we won't queue anything more
		task.setAttribute("status","ABORTED");
		task.setAttribute("error","Aborted on user request");

		error_tasks++;

		return;
	}

	// Get task name
	string task_name = task.getAttribute("name");

	// Get queue name (if present) and update task node
	string queue_name = task.getAttribute("queue");
	string task_host = task.getAttribute("host");

	task.setAttribute("status","QUEUED");

	QueuePool *pool = QueuePool::GetInstance();

	if(!pool->EnqueueTask(queue_name,task_host,this,task))
	{
		task.setAttribute("status","ABORTED");
		task.setAttribute("error","Unknown queue name");

		error_tasks++;

		Logger::Log(LOG_WARNING,"[WID %d] Unknown queue name '%s'",workflow_instance_id,queue_name.c_str());
		return;
	}

	running_tasks++;
	queued_tasks++;

	Logger::Log(LOG_INFO,"[WID %d] Added task %s to queue %s\n",workflow_instance_id,task_name.c_str(),queue_name.c_str());
	return;
}

void WorkflowInstance::schedule_update(DOMElement task,const string &schedule_name,int *retry_delay,int *retry_times)
{
	int schedule_levels;
	
	// Lookup for specified schedule
	unique_ptr<DOMXPathResult> res(xmldoc->evaluate("schedules/schedule[@name='"+schedule_name+"']",xmldoc->getDocumentElement(),DOMXPathResult::FIRST_RESULT_TYPE));
	if(!res->isNode())
	{
		Logger::Log(LOG_WARNING,"[WID %d] Unknown schedule : '%s'",workflow_instance_id,schedule_name.c_str());
		return;
	}
	
	DOMNode schedule = res->getNodeValue();

	unique_ptr<DOMXPathResult> res2(xmldoc->evaluate("count(level)",schedule,DOMXPathResult::FIRST_RESULT_TYPE));
	schedule_levels = res2->getIntegerValue();

	// Get current task schedule state
	int current_schedule_level;
	if(task.hasAttribute("retry_schedule_level"))
		current_schedule_level = XMLUtils::GetAttributeInt(task,"retry_schedule_level");
	else
		current_schedule_level = 0;

	current_schedule_level++;
	task.setAttribute("retry_schedule_level",to_string(current_schedule_level));

	if(current_schedule_level>schedule_levels)
	{
		*retry_delay = 0;
		*retry_times = 0;
		return; // We have reached the last schedule level
	}

	// Get schedule level details
	unique_ptr<DOMXPathResult> res3(xmldoc->evaluate("level[position()="+to_string(current_schedule_level)+"]",schedule,DOMXPathResult::FIRST_RESULT_TYPE));
	DOMElement schedule_level = (DOMElement)res3->getNodeValue();

	string retry_delay_str = schedule_level.getAttribute("retry_delay");
	string retry_times_str = schedule_level.getAttribute("retry_times");

	// Reset task retry informations, according to new shcedule level
	task.setAttribute("retry_delay",retry_delay_str);
	task.setAttribute("retry_times",retry_times_str);

	*retry_delay = std::stoi(retry_delay_str);
	*retry_times = std::stoi(retry_times_str);
}

bool WorkflowInstance::workflow_ended(void)
{
	if(running_tasks==0 && retrying_tasks==0)
	{
		// End workflow (and notify caller) if no tasks are queued or running at this point
		xmldoc->getDocumentElement().setAttribute("status","TERMINATED");
		if(error_tasks>0)
		{
			xmldoc->getDocumentElement().setAttribute("errors",to_string(error_tasks));

			Statistics::GetInstance()->IncWorkflowInstanceErrors();
		}

		return true;
	}

	return false;
}

void WorkflowInstance::record_savepoint(bool force)
{
	// Always update XML statistics
	update_statistics();

	// Also workflow XML attributes if necessary
	if(xmldoc->getDocumentElement().getAttribute("status")=="TERMINATED")
	{
		if(savepoint_level==0)
			return; // Even in forced mode we won't store terminated workflows on level 0

		xmldoc->getDocumentElement().setAttribute("end_time",format_datetime());
		xmldoc->getDocumentElement().setAttribute("errors",to_string(error_tasks));
	}
	else if(!force && savepoint_level<=2)
		return; // On level 1 and 2 we only record savepoints on terminated workflows

	string savepoint = xmldoc->Serialize(xmldoc->getDocumentElement());

	// Gather workflow values
	string workflow_instance_host, workflow_instance_start, workflow_instance_end, workflow_instance_status;
	if(xmldoc->getDocumentElement().hasAttribute("host"))
		workflow_instance_host = xmldoc->getDocumentElement().getAttribute("host");

	workflow_instance_start = xmldoc->getDocumentElement().getAttribute("start_time");

	if(xmldoc->getDocumentElement().hasAttribute("end_time"))
		workflow_instance_end = xmldoc->getDocumentElement().getAttribute("end_time");

	workflow_instance_status = xmldoc->getDocumentElement().getAttribute("status");

	int tries = 0;
	do
	{
		if(tries>0)
		{
			Logger::Log(LOG_WARNING,"[ WorkflowInstance::record_savepoint() ] Retrying in %d seconds\n",savepoint_retry_wait);
			sleep(savepoint_retry_wait);
		}

		try
		{
			DB db;

			if(savepoint_level>=2)
			{
				// Workflow has already been insterted into database, just update
				if(xmldoc->getDocumentElement().getAttribute("status")!="TERMINATED")
				{
					// Only update savepoint if workflow is still running
					db.QueryPrintfC("UPDATE t_workflow_instance SET workflow_instance_savepoint=%s WHERE workflow_instance_id=%i",savepoint.c_str(),&workflow_instance_id);
				}
				else
				{
					// Update savepoint and status if workflow is terminated
					db.QueryPrintfC("UPDATE t_workflow_instance SET workflow_instance_savepoint=%s,workflow_instance_status='TERMINATED',workflow_instance_errors=%i,workflow_instance_end=NOW() WHERE workflow_instance_id=%i",savepoint.c_str(),&error_tasks,&workflow_instance_id);
				}
			}
			else
			{
				// Always insert full informations as we are called at workflow end or when engine restarts
				db.QueryPrintfC("\
					INSERT INTO t_workflow_instance(workflow_instance_id,workflow_id,workflow_schedule_id,workflow_instance_host,workflow_instance_start,workflow_instance_end,workflow_instance_status,workflow_instance_errors,workflow_instance_savepoint)\
					VALUES(%i,%i,%i,%s,%s,%s,%s,%i,%s)",
					&workflow_instance_id,&workflow_id,&workflow_schedule_id,workflow_instance_host.length()?workflow_instance_host.c_str():0,workflow_instance_start.c_str(),workflow_instance_end.length()?workflow_instance_end.c_str():"0000-00-00 00:00:00",workflow_instance_status.c_str(),&error_tasks,savepoint.c_str());
			}

			break;
		}
		catch(Exception &e)
		{
			Logger::Log(LOG_ERR,"[ WorkflowInstance::record_savepoint() ] Unexpected exception : [ %s ] %s\n",e.context.c_str(),e.error.c_str());
		}

		tries++;

	} while(savepoint_retry && (savepoint_retry_times==0 || tries<=savepoint_retry_times));
}

string WorkflowInstance::format_datetime()
{
	time_t t;
	struct tm t_desc;

	t = time(0);
	localtime_r(&t,&t_desc);

	char str[64];
	sprintf(str,"%d-%02d-%02d %02d:%02d:%02d",1900+t_desc.tm_year,t_desc.tm_mon+1,t_desc.tm_mday,t_desc.tm_hour,t_desc.tm_min,t_desc.tm_sec);
	return string(str);
}

void WorkflowInstance::update_statistics()
{
	xmldoc->getDocumentElement().setAttribute("running_tasks",to_string(running_tasks));
	xmldoc->getDocumentElement().setAttribute("queued_tasks",to_string(queued_tasks));
	xmldoc->getDocumentElement().setAttribute("retrying_tasks",to_string(retrying_tasks));
	xmldoc->getDocumentElement().setAttribute("error_tasks",to_string(error_tasks));
}