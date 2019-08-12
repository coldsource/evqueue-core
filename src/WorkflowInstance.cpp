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
#include <ConfigurationEvQueue.h>
#include <WorkflowInstances.h>
#include <Logger.h>
#include <Task.h>
#include <RetrySchedules.h>
#include <RetrySchedule.h>
#include <SequenceGenerator.h>
#include <Notifications.h>
#include <Sockets.h>
#include <QueryResponse.h>
#include <XMLUtils.h>
#include <ProcessManager.h>
#include <Events.h>
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
	logs_directory(ConfigurationEvQueue::GetInstance()->Get("processmanager.logs.directory"))
{
	workflow_instance_id = 0;
	workflow_schedule_id = 0;

	log_dom_maxsize = ConfigurationEvQueue::GetInstance()->GetSize("datastore.dom.maxsize");

	saveparameters = ConfigurationEvQueue::GetInstance()->GetBool("workflowinstance.saveparameters");

	savepoint_level = ConfigurationEvQueue::GetInstance()->GetInt("workflowinstance.savepoint.level");
	if(savepoint_level<0 || savepoint_level>3)
		savepoint_level = 0;

	savepoint_retry = ConfigurationEvQueue::GetInstance()->GetBool("workflowinstance.savepoint.retry.enable");

	if(savepoint_retry)
	{
		savepoint_retry_times = ConfigurationEvQueue::GetInstance()->GetInt("workflowinstance.savepoint.retry.times");
		savepoint_retry_wait = ConfigurationEvQueue::GetInstance()->GetInt("workflowinstance.savepoint.retry.wait");
	}
	
	xmldoc = 0;

	is_cancelling = false;

	running_tasks = 0;
	queued_tasks = 0;
	retrying_tasks = 0;
	error_tasks = 0;
	waiting_conditions = 0;

	is_shutting_down = false;
}

WorkflowInstance::WorkflowInstance(const string &workflow_name,WorkflowParameters *parameters, unsigned int workflow_schedule_id,const string &workflow_host, const string &workflow_user, const string &workflow_comment):
	WorkflowInstance()
{
	DB db;

	if(workflow_name.length()>WORKFLOW_NAME_MAX_LEN)
		throw Exception("WorkflowInstance","Workflow name is too long","UNKNOWN_OBJECT");

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
	
	// Set workflow comment
	if(workflow_comment.length())
		xmldoc->getDocumentElement().setAttribute("comment",workflow_comment);

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
			throw Exception("WorkflowInstance","Unknown parameter : "+parameter_name,"INVALID_WORKFLOW_PARAMETERS");

		passed_parameters++;
	}

	{
		unique_ptr<DOMXPathResult> res(xmldoc->evaluate("count(parameters/parameter)",xmldoc->getDocumentElement(),DOMXPathResult::FIRST_RESULT_TYPE));
		int workflow_template_parameters = res->getIntegerValue();

		if(workflow_template_parameters!=passed_parameters)
		{
			char e[256];
			sprintf(e, "Invalid number of parameters passed to workflow (passed %d, expected %d)",passed_parameters,workflow_template_parameters);
			throw Exception("WorkflowInstance",e,"INVALID_WORKFLOW_PARAMETERS");
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
		db.QueryPrintf("INSERT INTO t_workflow_instance(node_name,workflow_id,workflow_schedule_id,workflow_instance_host,workflow_instance_status,workflow_instance_start,workflow_instance_comment) VALUES(%s,%i,%i,%s,'EXECUTING',NOW(),%s)",&ConfigurationEvQueue::GetInstance()->Get("cluster.node.name"),&workflow_id,workflow_schedule_id?&workflow_schedule_id:0,&workflow_host,&workflow_comment);
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
	
	// Upate XML ID (useful after workflows cloning)
	this->xmldoc->getDocumentElement().setAttribute("id",to_string(workflow_instance_id)); 

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
	
	Events::GetInstance()->Create(Events::en_types::INSTANCE_TERMINATED,workflow_instance_id);
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
	
	Events::GetInstance()->Create(Events::en_types::INSTANCE_STARTED,workflow_instance_id);
}

void WorkflowInstance::Resume(bool *workflow_terminated)
{
	QueuePool *qp = QueuePool::GetInstance();

	unique_lock<recursive_mutex> llock(lock);
	
	// Clear statistics
	clear_statistics();
	
	// Look for tasks or job that have conditions waiting for a new evaluation (via evqWait() function)
	{
		unique_ptr<DOMXPathResult> res(xmldoc->evaluate("//*[(name() = 'job' or name() = 'task') and @status='WAITING']",xmldoc->getDocumentElement(),DOMXPathResult::SNAPSHOT_RESULT_TYPE));
		int i = 0;
		while(res->snapshotItem(i++))
		{
			waiting_nodes.push_back((DOMElement)res->getNodeValue());
			waiting_conditions++;
			update_job_statistics("waiting_conditions",1,(DOMElement)res->getNodeValue());
		}
	}

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
			update_job_statistics("running_tasks",1,task);
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

void WorkflowInstance::DebugResume(bool *workflow_terminated)
{
	QueuePool *qp = QueuePool::GetInstance();

	unique_lock<recursive_mutex> llock(lock);
	
	// Put instance back in executing status
	xmldoc->getDocumentElement().setAttribute("status","EXECUTING");
	
	// Clear statistics
	clear_statistics();
	
	// Look for tasks or job that have conditions waiting for a new evaluation (via evqWait() function)
	{
		unique_ptr<DOMXPathResult> res(xmldoc->evaluate("//*[(name() = 'job' or name() = 'task') and @status='WAITING']",xmldoc->getDocumentElement(),DOMXPathResult::SNAPSHOT_RESULT_TYPE));
		int i = 0;
		while(res->snapshotItem(i++))
		{
			waiting_nodes.push_back((DOMElement)res->getNodeValue());
			waiting_conditions++;
			update_job_statistics("waiting_conditions",1,(DOMElement)res->getNodeValue());
		}
	}
	
	// Look for TERMINATED tasks (in error) and relaunch them. Also requeue QUEUED tasks
	int tasks_index = 0;
	{
		unique_ptr<DOMXPathResult> tasks(xmldoc->evaluate("//task[(@status='TERMINATED' and @retval!=0) or @status='QUEUED']",xmldoc->getDocumentElement(),DOMXPathResult::SNAPSHOT_RESULT_TYPE));
		DOMElement task;

		while(tasks->snapshotItem(tasks_index++))
		{
			task = (DOMElement)tasks->getNodeValue();
			
			if(task.getAttribute("status")=="TERMINATED")
			{
				// Reset task attributes
				task.removeAttribute("retry_schedule_level");
				task.removeAttribute("retry_times");
				task.removeAttribute("retry_at");
				task.removeAttribute("progression");
				task.removeAttribute("error");
				task.removeAttribute("retval");
				task.removeAttribute("retval");
				task.removeAttribute("execution_time");
			}
			
			enqueue_task(task);
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
			update_job_statistics("running_tasks",1,task);
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
	Configuration *config = ConfigurationEvQueue::GetInstance();

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

bool WorkflowInstance::handle_condition(DOMElement node,DOMElement context_node,bool can_wait)
{
	if(!node.hasAttribute("condition"))
		return true;

	ExceptionWorkflowContext ctx(node,"Error evaluating condition");
	
	bool needs_wait = false;
	xmldoc->getXPath()->RegisterFunction("evqWait",{WorkflowXPathFunctions::evqWait,&needs_wait});
	
	try
	{
		unique_ptr<DOMXPathResult> test_expr(xmldoc->evaluate(node.getAttribute("condition"),context_node,DOMXPathResult::BOOLEAN_TYPE));
		
		if(!test_expr->getBooleanValue())
		{
			node.setAttribute("status","SKIPPED");
			node.setAttribute("details","Condition evaluates to false");
			return false;
		}
		
		node.removeAttribute("condition");
		node.setAttribute("details","Condition evaluates to true");
		
		return true;
	}
	catch(Exception &e)
	{
		if(needs_wait)
		{
			if(!can_wait)
				throw;
			
			// evqWait() has thrown exception because it needs to wait
			waiting_nodes.push_back(node);
			node.setAttribute("status","WAITING");
			node.setAttribute("details","Waiting for condition to become true");
			waiting_conditions++;
			update_job_statistics("waiting_conditions",1,node);
			return false;
		}
		else
			throw; // Syntax or dynamic exception (ie real error)
	}
}

bool WorkflowInstance::handle_loop(DOMElement node,DOMElement context_node,vector<DOMElement> &nodes, vector<DOMElement> &contexts)
{
	// Check for loop (must expand them before execution)
	if(!node.hasAttribute("loop"))
	{
		nodes.push_back(node);
		contexts.push_back(context_node);
		return false;
	}

	ExceptionWorkflowContext ctx(node,"Error evaluating loop");
	
	string loop_xpath = node.getAttribute("loop");
	node.removeAttribute("loop");
	
	string iteration_condition;
	if(node.hasAttribute("iteration-condition"))
	{
		iteration_condition = node.getAttribute("iteration-condition");
		node.removeAttribute("iteration-condition");
	}
	
	// This is unchecked user input, try evaluation
	DOMNode matching_node;
	unique_ptr<DOMXPathResult> matching_nodes(xmldoc->evaluate(loop_xpath,context_node,DOMXPathResult::SNAPSHOT_RESULT_TYPE));
	
	int matching_nodes_index = 0;
	while(matching_nodes->snapshotItem(matching_nodes_index++))
	{
		matching_node = matching_nodes->getNodeValue();

		DOMNode node_clone = node.cloneNode(true);
		
		if(iteration_condition!="")
			((DOMElement)node_clone).setAttribute("condition",iteration_condition);
		
		node.getParentNode().appendChild(node_clone);
		
		nodes.push_back(node_clone);
		contexts.push_back(matching_node);
	}

	node.getParentNode().removeChild(node);
	
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

void WorkflowInstance::replace_values(DOMElement task,DOMElement context_node)
{
	// Expand dynamic task host if needed
	if(task.hasAttribute("host"))
	{
		ExceptionWorkflowContext ctx(task,"Error computing dynamic host");
		
		string attr_val = task.getAttribute("host");
		string expanded_attr_val = xmldoc->ExpandXPathAttribute(attr_val,context_node);
		task.setAttribute("host",expanded_attr_val);
	}
	
	// Expand dynamic queue host if needed
	if(task.hasAttribute("queue_host"))
	{
		ExceptionWorkflowContext ctx(task,"Error computing dynamic queue host");
		
		string attr_val = task.getAttribute("queue_host");
		string expanded_attr_val = xmldoc->ExpandXPathAttribute(attr_val,context_node);
		task.setAttribute("queue_host",expanded_attr_val);
	}
	
	// Expand dynamic task user if needed
	if(task.hasAttribute("user"))
	{
		ExceptionWorkflowContext ctx(task,"Error computing dynamic user");
		
		string attr_val = task.getAttribute("user");
		std::string expanded_attr_val = xmldoc->ExpandXPathAttribute(attr_val,context_node);
		task.setAttribute("user",expanded_attr_val);
	}
	
	unique_ptr<DOMXPathResult> inputs(xmldoc->evaluate("./input",task,DOMXPathResult::SNAPSHOT_RESULT_TYPE));
	int inputs_index = 0;
	DOMElement input;
	while(inputs->snapshotItem(inputs_index++))
	{
		input = (DOMElement)inputs->getNodeValue();
		replace_value(input,context_node);
	}
	
	unique_ptr<DOMXPathResult> stdin(xmldoc->evaluate("./stdin",task,DOMXPathResult::FIRST_RESULT_TYPE));
	if(stdin->isNode())
		replace_value(stdin->getNodeValue(),context_node);
	
	unique_ptr<DOMXPathResult> script(xmldoc->evaluate("./script",task,DOMXPathResult::FIRST_RESULT_TYPE));
	if(script->isNode())
		replace_value(script->getNodeValue(),context_node);
}

void WorkflowInstance::replace_value(DOMElement input,DOMElement context_node)
{
	DOMElement value;
	int values_index;
	
	{
		ExceptionWorkflowContext ctx(input,"Error computing condition");
		
		if(!handle_condition(input,context_node,false))
			return;
	}
	
	vector<DOMElement> inputs;
	vector<DOMElement> contexts;
	handle_loop(input,context_node,inputs,contexts);
	
	for(int i=0;i<inputs.size();i++)
	{
		DOMElement current_input = inputs.at(i);
		
		{
			ExceptionWorkflowContext ctx(current_input,"Error computing input value");
			
			// Replace <value> nodes par their literal value
			unique_ptr<DOMXPathResult> values(xmldoc->evaluate(".//value",current_input,DOMXPathResult::SNAPSHOT_RESULT_TYPE));
			values_index = 0;
			while(values->snapshotItem(values_index++))
			{
				value = (DOMElement)values->getNodeValue();

				// This is unchecked user input. We have to try evaluation
				unique_ptr<DOMXPathResult> value_nodes(xmldoc->evaluate(value.getAttribute("select"),contexts.at(i),DOMXPathResult::FIRST_RESULT_TYPE));
				
				if(value_nodes->isNode())
					value.getParentNode().replaceChild(xmldoc->createTextNode(value_nodes->getStringValue()),value);
			}
		}
		
		{
			ExceptionWorkflowContext ctx(current_input,"Error computing input values from copy node");
			
			// Replace <copy> nodes par their value
			unique_ptr<DOMXPathResult> values(xmldoc->evaluate(".//copy",current_input,DOMXPathResult::SNAPSHOT_RESULT_TYPE));
			values_index = 0;
			while(values->snapshotItem(values_index++))
			{
				value = (DOMElement)values->getNodeValue();
				string xpath_select = value.getAttribute("select");

				// This is unchecked user input. We have to try evaluation
				unique_ptr<DOMXPathResult> result_nodes(xmldoc->evaluate(xpath_select,contexts.at(i),DOMXPathResult::SNAPSHOT_RESULT_TYPE));
				
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
	return;
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
			update_job_statistics("error_tasks",1,task);
			return;
		}

		retry_delay = XMLUtils::GetAttributeInt(task,"retry_delay");

		if(!task.hasAttribute("retry_times"))
		{
			error_tasks++; // We won't retry this task, set error
			update_job_statistics("error_tasks",1,task);
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
		update_job_statistics("error_tasks",1,task);
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
	update_job_statistics("retrying_tasks",1,task);
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

void WorkflowInstance::record_log(DOMElement node, const char *log)
{
	if(strlen(log)<log_dom_maxsize)
	{
		node.appendChild(xmldoc->createTextNode(log));
	}
	else
	{
		DB db;
		try
		{
			db.QueryPrintfC("INSERT INTO t_datastore(workflow_instance_id,datastore_value) VALUES(%i,%s)",&workflow_instance_id,log);
			
			int datastore_id = db.InsertID();
			node.setAttribute("datastore-id",to_string(datastore_id));
		}
		catch(Exception &e)
		{
			node.setAttribute("datastore-error",e.error);
		}
	}
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

void WorkflowInstance::update_job_statistics(const string &name,int delta,DOMElement node)
{
	DOMElement job;
	string node_name = node.getNodeName();
	if(node_name=="task")
		job = node.getParentNode().getParentNode();
	else if(node_name=="job" || node_name=="workflow")
		job = node;
	else
		return;
	
	int oldval = job.hasAttribute(name)?stoi(job.getAttribute(name)):0;
	job.setAttribute(name,to_string(oldval+delta));
	if(job.getParentNode() && job.getParentNode().getParentNode())
		update_job_statistics(name,delta,job.getParentNode().getParentNode());
}

void WorkflowInstance::clear_statistics()
{
	// Clear statistics
	{
		unique_ptr<DOMXPathResult> res(xmldoc->evaluate("//*[name() = 'job' or name() = 'task' or name() = 'workflow']",xmldoc->getDocumentElement(),DOMXPathResult::SNAPSHOT_RESULT_TYPE));
		int i = 0;
		while(res->snapshotItem(i++))
		{
			DOMElement node = (DOMElement)res->getNodeValue();
			node.removeAttribute("running_tasks");
			node.removeAttribute("queued_tasks");
			node.removeAttribute("retrying_tasks");
			node.removeAttribute("error_tasks");
			node.removeAttribute("waiting_conditions");
		}
	}
}
