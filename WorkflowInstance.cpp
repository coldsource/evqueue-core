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
#include <QueuePool.h>
#include <DB.h>
#include <Exception.h>
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
#include <tools.h>
#include <global.h>

#include <xqilla/xqilla-dom3.hpp>

#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <sys/socket.h>

extern pthread_mutex_t main_mutex;
extern pthread_cond_t fork_lock;
extern bool fork_locked,fork_possible;

using namespace std;

WorkflowInstance::WorkflowInstance(void):
	logs_directory(Configuration::GetInstance()->Get("processmanager.logs.directory")),
	errlogs_directory(Configuration::GetInstance()->Get("processmanager.errlogs.directory")),
	tasks_directory(Configuration::GetInstance()->Get("processmanager.tasks.directory")),
	monitor_path(Configuration::GetInstance()->Get("processmanager.monitor.path"))
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
	
	is_cancelling = false;
	
	running_tasks = 0;
	queued_tasks = 0;
	retrying_tasks = 0;
	error_tasks = 0;
	
	parser = 0;
	serializer = 0;
	
	is_shutting_down = false;
	
	// Initialize mutex
	pthread_mutexattr_t lock_attr;
	pthread_mutexattr_init(&lock_attr);
	pthread_mutexattr_settype(&lock_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&lock, &lock_attr);
}

WorkflowInstance::WorkflowInstance(const char *workflow_name,WorkflowParameters *parameters, unsigned int workflow_schedule_id,const char *workflow_host, const char *workflow_user):
	WorkflowInstance()
{
	DB db;
	char buf[256+WORKFLOW_NAME_MAXLEN];
	
	if(strlen(workflow_name)>WORKFLOW_NAME_MAXLEN)
		throw Exception("WorkflowInstance","Workflow name is too long");

	// Get workflow. This will throw an exception if workflow doesn't exist
	Workflow workflow = Workflows::GetInstance()->GetWorkflow(workflow_name);

	workflow_id = workflow.GetID();
	notifications = workflow.GetNotifications();
	
	this->workflow_schedule_id = workflow_schedule_id;
	
	// Load workflow XML
	DOMImplementation *xqillaImplementation = DOMImplementationRegistry::getDOMImplementation(X("XPath2 3.0"));
	parser = xqillaImplementation->createLSParser(DOMImplementationLS::MODE_SYNCHRONOUS,0);
	serializer = xqillaImplementation->createLSSerializer();
	
	DOMLSInput *input = xqillaImplementation->createLSInput();
	
	// Set XML content and parse document
	XMLCh *xml;
	xml = XMLString::transcode(workflow.GetXML());
	input->setStringData(xml);
	xmldoc = parser->parse(input);
	
	input->release();
	
	XMLString::release(&xml);
	
	// Create resolver
	resolver = xmldoc->createNSResolver(xmldoc->getDocumentElement());
	resolver->addNamespaceBinding(X("xs"), X("http://www.w3.org/2001/XMLSchema"));
	
	// Set workflow name for front-office display
	xmldoc->getDocumentElement()->setAttribute(X("name"),X(workflow_name));
	
	// Set workflow user and host
	if(workflow_host)
	{
		xmldoc->getDocumentElement()->setAttribute(X("host"),X(workflow_host));
		
		if(workflow_user)
			xmldoc->getDocumentElement()->setAttribute(X("user"),X(workflow_user));
	}
	
	// Set input parameters
	const char *parameter_name;
	const char *parameter_value;
	int passed_parameters = 0;
	char buf2[256+PARAMETER_NAME_MAX_LEN];
	
	parameters->SeekStart();
	while(parameters->Get(&parameter_name,&parameter_value))
	{
		sprintf(buf2,"parameters/parameter[@name = '%s']",parameter_name);
		DOMXPathResult *res = xmldoc->evaluate(X(buf2),xmldoc->getDocumentElement(),resolver,DOMXPathResult::FIRST_RESULT_TYPE,0);
		if(res->isNode())
		{
			DOMNode *parameter_node = res->getNodeValue();
			parameter_node->setTextContent(X(parameter_value));
		}
		else
		{
			res->release();
			sprintf(buf2,"Unknown parameter : %s",parameter_name);
			throw Exception("WorkflowInstance",buf2);
		}
		
		res->release();
		passed_parameters++;
	}
	
	DOMXPathResult *res = xmldoc->evaluate(X("count(parameters/parameter)"),xmldoc->getDocumentElement(),resolver,DOMXPathResult::FIRST_RESULT_TYPE,0);
	int workflow_template_parameters = res->getIntegerValue();
	res->release();
	
	if(workflow_template_parameters!=passed_parameters)
	{
		char e[256];
		sprintf(e, "Invalid number of parameters passed to workflow (passed %d, expected %d)",passed_parameters,workflow_template_parameters);
		throw Exception("WorkflowInstance",e);
	}
	
	// Import schedule
	DOMXPathResult *tasks = xmldoc->evaluate(X("//task[@retry_schedule]"),xmldoc->getDocumentElement(),resolver,DOMXPathResult::SNAPSHOT_RESULT_TYPE,0);
	
	DOMNode *task;
	unsigned int tasks_index = 0;
	while(tasks->snapshotItem(tasks_index++))
	{
		task = tasks->getNodeValue();
		
		const XMLCh *schedule_name = ((DOMElement *)task)->getAttribute(X("retry_schedule"));
		char *schedule_name_c = XMLString::transcode(schedule_name);
		
		sprintf(buf,"schedules/schedule[@name = '%s']",schedule_name_c);
		
		DOMXPathResult *res = xmldoc->evaluate(X(buf),xmldoc->getDocumentElement(),resolver,DOMXPathResult::FIRST_RESULT_TYPE,0);
		if(!res->isNode())
		{
			// Schedule is not local, check global
			RetrySchedule retry_schedule;
			try
			{
				retry_schedule = RetrySchedules::GetInstance()->GetRetrySchedule(schedule_name_c);
			}
			catch(Exception &e)
			{
				res->release();
				tasks->release();
				XMLString::release(&schedule_name_c);
				
				throw e;
			}
			
			// Import global schedule
			DOMLSParser *schedule_parser = xqillaImplementation->createLSParser(DOMImplementationLS::MODE_SYNCHRONOUS,0);
			DOMLSInput *schedule_input = xqillaImplementation->createLSInput();
			
			// Load schedule XML data
			XMLCh *schedule_xml;
			schedule_xml = XMLString::transcode(retry_schedule.GetXML());
			schedule_input->setStringData(schedule_xml);
			
			DOMDocument *schedule_xmldoc = schedule_parser->parse(schedule_input);
			
			// Add schedule to current workflow
			DOMNode * schedule_node = xmldoc->importNode(schedule_xmldoc->getDocumentElement(),true);
			
			DOMXPathResult *res = xmldoc->evaluate(X("schedules"),xmldoc->getDocumentElement(),resolver,DOMXPathResult::FIRST_RESULT_TYPE,0);
			DOMNode *schedules_nodes;
			if(res->isNode())
				schedules_nodes = res->getNodeValue();
			else
			{
				schedules_nodes = xmldoc->createElement(X("schedules"));
				xmldoc->getDocumentElement()->appendChild(schedules_nodes);
			}
			res->release();
			
			schedules_nodes->appendChild(schedule_node);
			
			
			schedule_input->release();
			schedule_parser->release();
			
			XMLString::release(&schedule_xml);
		}
		
		XMLString::release(&schedule_name_c);
		res->release();
	}
	
	tasks->release();
	
	if(savepoint_level>=2)
	{
		// Insert workflow instance in DB
		db.QueryPrintf("INSERT INTO t_workflow_instance(node_name,workflow_id,workflow_schedule_id,workflow_instance_host,workflow_instance_status,workflow_instance_start) VALUES(%s,%i,%i,%s,'EXECUTING',NOW())",Configuration::GetInstance()->Get("network.node.name").c_str(),&workflow_id,workflow_schedule_id?&workflow_schedule_id:0,workflow_host);
		this->workflow_instance_id = db.InsertID();
		
		// Save workflow parameters
		if(saveparameters)
		{
			parameters->SeekStart();
			while(parameters->Get(&parameter_name,&parameter_value))
				db.QueryPrintf("INSERT INTO t_workflow_instance_parameters VALUES(%i,%s,%s)",&this->workflow_instance_id,parameter_name,parameter_value);
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
	
	if(!db.GetField(0) || strlen(db.GetField(0))==0)
	{
		db.QueryPrintf("UPDATE t_workflow_instance SET workflow_instance_status='TERMINATED' WHERE workflow_instance_id=%i",&workflow_instance_id);
		
		throw Exception("WorkflowInstance","Could not resume workflow : empty savepoint");
	}
	
	this->workflow_instance_id = workflow_instance_id;
	
	// Load workflow XML
	DOMImplementation *xqillaImplementation = DOMImplementationRegistry::getDOMImplementation(X("XPath2 3.0"));
	parser = xqillaImplementation->createLSParser(DOMImplementationLS::MODE_SYNCHRONOUS,0);
	serializer = xqillaImplementation->createLSSerializer();
	
	DOMLSInput *input = xqillaImplementation->createLSInput();
	
	// Set XML content and parse document
	XMLCh *xml;
	xml = XMLString::transcode(db.GetField(0));
	input->setStringData(xml);
	xmldoc = parser->parse(input);
	
	input->release();
	
	XMLString::release(&xml);
	
	// Create resolver
	resolver = xmldoc->createNSResolver(xmldoc->getDocumentElement());
	resolver->addNamespaceBinding(X("xs"), X("http://www.w3.org/2001/XMLSchema"));
	
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
		
		// Unregister new instance to ensure noone is still using it
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
	
	if(parser)
		parser->release();
	
	if(serializer)
		serializer->release();
	
	Logger::Log(LOG_NOTICE,"[WID %d] Terminated",workflow_instance_id);
}

void WorkflowInstance::Start(bool *workflow_terminated)
{
	pthread_mutex_lock(&lock);
	
	Logger::Log(LOG_NOTICE,"[WID %d] Started",workflow_instance_id);
	
	char workflow_instance_id_str[10];
	char buf[32];
	sprintf(workflow_instance_id_str,"%d",workflow_instance_id);
	xmldoc->getDocumentElement()->setAttribute(X("id"),X(workflow_instance_id_str));
	xmldoc->getDocumentElement()->setAttribute(X("status"),X("EXECUTING"));
	format_datetime(buf);
	xmldoc->getDocumentElement()->setAttribute(X("start_time"),X(buf));
	
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
	
	pthread_mutex_unlock(&lock);
}

void WorkflowInstance::Resume(bool *workflow_terminated)
{
	char buf[256];
	int tasks_index = 0;
	
	QueuePool *qp = QueuePool::GetInstance();
	
	pthread_mutex_lock(&lock);
	
	// Look for EXECUTING tasks (they might be still in execution or terminated)
	// Also look for TERMINATED tasks because we might have to retry them
	DOMXPathResult *tasks = xmldoc->evaluate(X("//task[@status='QUEUED' or @status='EXECUTING' or @status='TERMINATED']"),xmldoc->getDocumentElement(),resolver,DOMXPathResult::SNAPSHOT_RESULT_TYPE,0);
	DOMNode *task;
	
	while(tasks->snapshotItem(tasks_index++))
	{
		task = tasks->getNodeValue();
		
		if(XMLString::compareString(X("QUEUED"),((DOMElement *)task)->getAttribute(X("status")))==0)
			enqueue_task(task);
		else if(XMLString::compareString(X("EXECUTING"),((DOMElement *)task)->getAttribute(X("status")))==0)
		{
			// Restore mapping tables in QueuePool for executing tasks
			char *queue_name = XMLString::transcode(((DOMElement *)task)->getAttribute(X("queue")));
			pid_t task_id = XMLString::parseInt(((DOMElement *)task)->getAttribute(X("tid")));
			qp->ExecuteTask(this,task,queue_name,task_id);
			XMLString::release(&queue_name);
			
			running_tasks++;
		}
		else if(XMLString::compareString(X("TERMINATED"),((DOMElement *)task)->getAttribute(X("status")))==0)
		{
			if(XMLString::compareString(X("0"),((DOMElement *)task)->getAttribute(X("retval")))!=0)
				retry_task(task);
		}
	}
	
	tasks->release();
	
	*workflow_terminated = workflow_ended();
	
	if(tasks_index>0)
		record_savepoint(); // XML has been modified (tasks status update from DB)
	
	pthread_mutex_unlock(&lock);
}

void WorkflowInstance::Migrate(bool *workflow_terminated)
{
	int tasks_index = 0;
	
	pthread_mutex_lock(&lock);
	
	// For EXECUTING tasks : since we are migrating from another node, they will never end, fake termination
	DOMXPathResult *tasks = xmldoc->evaluate(X("//task[@status='QUEUED' or @status='EXECUTING' or @status='TERMINATED']"),xmldoc->getDocumentElement(),resolver,DOMXPathResult::SNAPSHOT_RESULT_TYPE,0);
	DOMNode *task;
	
	while(tasks->snapshotItem(tasks_index++))
	{
		task = tasks->getNodeValue();
		
		if(XMLString::compareString(X("QUEUED"),((DOMElement *)task)->getAttribute(X("status")))==0)
			enqueue_task(task);
		else if(XMLString::compareString(X("EXECUTING"),((DOMElement *)task)->getAttribute(X("status")))==0)
		{
			// Fake task ending with generic error code
			running_tasks++; // Inc running_tasks before calling TaskStop
			TaskStop(task,-1,"Task migrated",0,0,workflow_terminated);
		}
		else if(XMLString::compareString(X("TERMINATED"),((DOMElement *)task)->getAttribute(X("status")))==0)
		{
			if(XMLString::compareString(X("0"),((DOMElement *)task)->getAttribute(X("retval")))!=0)
				retry_task(task);
		}
	}
	
	tasks->release();
	
	*workflow_terminated = workflow_ended();
	
	if(tasks_index>0)
		record_savepoint(); // XML has been modified (tasks status update from DB)
	
	// Workflow is migrated, update node name
	Configuration *config = Configuration::GetInstance();
	
	DB db;
	db.QueryPrintf("UPDATE t_workflow_instance SET node_name=%s WHERE workflow_instance_id=%i",config->Get("network.node.name").c_str(),&workflow_instance_id);
	
	pthread_mutex_unlock(&lock);
}

void WorkflowInstance::Cancel()
{
	Logger::Log(LOG_NOTICE,"[WID %d] Cancelling",workflow_instance_id);
	
	pthread_mutex_lock(&lock);
	
	is_cancelling = true;
	
	pthread_mutex_unlock(&lock);
}

void WorkflowInstance::Shutdown(void)
{
	is_shutting_down = true;
}

void WorkflowInstance::TaskRestart(DOMNode *task, bool *workflow_terminated)
{
	pthread_mutex_lock(&lock);
	
	enqueue_task(task);
	retrying_tasks--;
	
	*workflow_terminated = workflow_ended();
	
	record_savepoint();
	
	pthread_mutex_unlock(&lock);
}

bool WorkflowInstance::TaskStop(DOMNode *task_node,int retval,const char *stdout_output,const char *stderr_output,const char *log_output,bool *workflow_terminated)
{
	char buf[256];
	
	pthread_mutex_lock(&lock);
	
	sprintf(buf,"%d",retval);
	((DOMElement *)task_node)->setAttribute(X("status"),X("TERMINATED"));
	((DOMElement *)task_node)->setAttribute(X("retval"),X(buf));
	((DOMElement *)task_node)->removeAttribute(X("tid"));
	((DOMElement *)task_node)->removeAttribute(X("pid"));
	
	TaskUpdateProgression(task_node,100);
	
	// Generate output node
	DOMElement *output_element = xmldoc->createElement(X("output"));
	
	output_element->setAttribute(X("retval"),X(buf));
	
	output_element->setAttribute(X("execution_time"),(((DOMElement *)task_node)->getAttribute(X("execution_time"))));

	format_datetime(buf);
	output_element->setAttribute(X("exit_time"),X(buf));
	
	if(retval==0)
	{
		char *task_name_c = XMLString::transcode(((DOMElement *)task_node)->getAttribute(X("name")));
		Task task;
		
		try
		{
			task = Tasks::GetInstance()->GetTask(task_name_c);
		}
		catch(Exception e)
		{
			((DOMElement *)task_node)->setAttribute(X("status"),X("ABORTED"));
			((DOMElement *)task_node)->setAttribute(X("error"),X("Task has vanished"));
			
			error_tasks++;
		}
		
		XMLString::release(&task_name_c);
		
		if(task.GetOutputMethod()==task_output_method::XML)
		{
			// Import task output in current document (XML mode)
			DOMImplementation *xqillaImplementation = DOMImplementationRegistry::getDOMImplementation(X("XPath2 3.0"));
			DOMLSParser *parser = xqillaImplementation->createLSParser(DOMImplementationLS::MODE_SYNCHRONOUS,0);
			DOMLSInput *input = xqillaImplementation->createLSInput();
			
			// Set XML content and parse document
			XMLCh *xml = XMLString::transcode(stdout_output);
			input->setStringData(xml);
			
			DOMDocument *output_xmldoc = parser->parse(input);
			if(output_xmldoc && output_xmldoc->getDocumentElement())
			{
				// XML is valid
				output_element->setAttribute(X("method"),X("xml"));
				output_element->appendChild(xmldoc->importNode(output_xmldoc->getDocumentElement(),true));
				task_node->appendChild(output_element);
			}
			else
			{
				// Task returned successfully but XML is invalid. Unable to continue as following tasks might need XML output
				if(errlogs)
				{
					char *errlog_filename = new char[errlogs_directory.length()+32];
					sprintf(errlog_filename,"%s/%d-XXXXXX.log",errlogs_directory.c_str(),workflow_instance_id);
					
					int fno = mkstemps(errlog_filename,4);
					write(fno,stdout_output,strlen(stdout_output));
					close(fno);
					
					Logger::Log(LOG_WARNING,"[WID %d] Invalid XML returned, output has been saved as %s",workflow_instance_id,errlog_filename);
					
					delete[] errlog_filename;
				}
				
				((DOMElement *)task_node)->setAttribute(X("status"),X("ABORTED"));
				((DOMElement *)task_node)->setAttribute(X("error"),X("Invalid XML returned"));
				
				error_tasks++;
			}
			
			XMLString::release(&xml);
			input->release();
			parser->release();
		}
		else if(task.GetOutputMethod()==task_output_method::TEXT)
		{
			// We are in text mode
			output_element->setAttribute(X("method"),X("text"));
			XMLCh *out = XMLString::transcode(stdout_output);
			output_element->appendChild(xmldoc->createTextNode(out));
			task_node->appendChild(output_element);
			XMLString::release(&out);
		}
	}
	else
	{
		// Store task log in output node. We treat this as TEXT since errors can corrupt XML
		output_element->setAttribute(X("method"),X("text"));
		XMLCh *out = XMLString::transcode(stdout_output);
		output_element->appendChild(xmldoc->createTextNode(out));
		task_node->appendChild(output_element);
		XMLString::release(&out);
		
		if(is_cancelling)
		{
			// We are in cancelling state, we won't execute anything more
			((DOMElement *)task_node)->setAttribute(X("error"),X("Won't retry because workflow is cancelling"));
		}
		else
			retry_task(task_node);
	}
	
	// Add stderr output if present
	if(stderr_output)
	{
		DOMElement *stderr_element = xmldoc->createElement(X("stderr"));
		
		XMLCh *out = XMLString::transcode(stderr_output);
		stderr_element->appendChild(xmldoc->createTextNode(out));
		task_node->appendChild(stderr_element);
		XMLString::release(&out);
	}
	
	// Add evqueue log output if present
	if(log_output)
	{
		DOMElement *log_element = xmldoc->createElement(X("log"));
		
		XMLCh *out = XMLString::transcode(log_output);
		log_element->appendChild(xmldoc->createTextNode(out));
		task_node->appendChild(log_element);
		XMLString::release(&out);
	}
	
	DOMXPathResult *res;
	int tasks_count,tasks_successful;
	
	res = xmldoc->evaluate(X("count(../task)"),task_node,resolver,DOMXPathResult::FIRST_RESULT_TYPE,0);
	tasks_count = res->getIntegerValue();
	res->release();
	
	res = xmldoc->evaluate(X("count(../task[(@status='TERMINATED' and @retval = 0) or (@status='SKIPPED')])"),task_node,resolver,DOMXPathResult::FIRST_RESULT_TYPE,0);
	tasks_successful = res->getIntegerValue();
	res->release();
	
	if(tasks_count==tasks_successful)
	{
		DOMNode *job = task_node->getParentNode()->getParentNode();
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
	
	pthread_mutex_unlock(&lock);
	
	return true;
}

pid_t WorkflowInstance::TaskExecute(DOMNode *task_node,pid_t tid,bool *workflow_terminated)
{
	char buf[32],tid_str[16];
	char *task_name_c;
	int parameters_pipe[2];
	Task task;
	
	pthread_mutex_lock(&lock);
	
	// As we arrive here, the task is queued. Whatever comes, its status will not be queued anymore (will be executing or aborted)
	queued_tasks--;
	
	if(is_cancelling)
	{
		// We are in cancelling state, we won't execute anything more
		((DOMElement *)task_node)->setAttribute(X("status"),X("ABORTED"));
		((DOMElement *)task_node)->setAttribute(X("error"),X("Aborted on user request"));
		
		error_tasks++;
		running_tasks--;
		
		*workflow_terminated = workflow_ended();
		
		record_savepoint();
		
		pthread_mutex_unlock(&lock);
		return -1;
	}
	
	// Get task informations
	task_name_c = XMLString::transcode(((DOMElement *)task_node)->getAttribute(X("name")));
	
	try
	{
		task = Tasks::GetInstance()->GetTask(task_name_c);
	}
	catch(Exception e)
	{
		// Task not fount, abort and set error message
		((DOMElement *)task_node)->setAttribute(X("status"),X("ABORTED"));
		((DOMElement *)task_node)->setAttribute(X("error"),X("Unknown task name"));
		
		error_tasks++;
		running_tasks--;
		
		*workflow_terminated = workflow_ended();
		
		record_savepoint();
		
		Logger::Log(LOG_WARNING,"[WID %d] Unknown task name '%s'",workflow_instance_id,task_name_c);
		
		XMLString::release(&task_name_c);
		
		pthread_mutex_unlock(&lock);
		return -1;
	}
	
	Logger::Log(LOG_NOTICE,"[WID %d] Executing %s",workflow_instance_id,task_name_c);
	
	XMLString::release(&task_name_c);
	
	
	// Prepare parameters
	// This must be done before fork() because xerces Transcode() can deadlock on fork (xerces bug...)
	unsigned int parameters_count,parameters_index;
	char **parameters_name,**parameters_value;
	
	DOMXPathResult *res;
	res = xmldoc->evaluate(X("count(input)"),task_node,resolver,DOMXPathResult::FIRST_RESULT_TYPE,0);
	parameters_count = res->getIntegerValue();
	res->release();
	
	parameters_name = new char*[parameters_count];
	parameters_value = new char*[parameters_count];
	
	DOMXPathResult *parameters = xmldoc->evaluate(X("input"),task_node,resolver,DOMXPathResult::SNAPSHOT_RESULT_TYPE,0);
	DOMNode *parameter;
	parameters_index = 0;
	
	while(parameters->snapshotItem(parameters_index))
	{
		parameter = parameters->getNodeValue();
		
		if(parameter->getAttributes()->getNamedItem(X("name")))
			parameters_name[parameters_index] = XMLString::transcode(parameter->getAttributes()->getNamedItem(X("name"))->getNodeValue());
		else
			parameters_name[parameters_index] = 0;
		
		parameters_value[parameters_index] = XMLString::transcode(parameter->getTextContent());
		
		parameters_index++;
	}
	
	parameters->release();
	
	// Prepare pipe for SDTIN
	char *stdin_parameter_c = 0;
	
	parameters = xmldoc->evaluate(X("stdin"),task_node,resolver,DOMXPathResult::FIRST_RESULT_TYPE,0);
	
	if(parameters->isNode())
	{
		DOMElement *stdin_node = (DOMElement *)parameters->getNodeValue();
		if(stdin_node->hasAttribute(X("mode")) && XMLString::compareString(X("text"),stdin_node->getAttribute(X("mode")))==0)
		{
			const XMLCh *stdin_parameter = stdin_node->getTextContent();
			stdin_parameter_c = XMLString::transcode(stdin_parameter);
		}
		else
		{
			XMLCh *stdin_parameter = serializer->writeToString(stdin_node);
			stdin_parameter_c = XMLString::transcode(stdin_parameter);
			XMLString::release(&stdin_parameter);
		}
	}
	
	parameters->release();
	
	if(stdin_parameter_c)
		pipe(parameters_pipe); // Prepare pipe before fork() since we have STDIN input
	
	// Fork child
	pid_t pid = fork();
	
	if(pid==0)
	{
		setsid(); // This is used to avoid CTRL+C killing all child processes
		
		pid = getpid();
		sprintf(tid_str,"%d",tid);
		
		// Send QID to monitor
		Configuration *config = Configuration::GetInstance();
		setenv("EVQUEUE_IPC_QID",config->Get("core.ipc.qid").c_str(),true);
		
		// Compute task filename
		string task_filename;
		if(task.IsAbsolutePath())
			task_filename = task.GetBinary();
		else
			task_filename = tasks_directory+"/"+task.GetBinary();
		
		if(!task.GetWorkingDirectory().empty())
			setenv("EVQUEUE_WORKING_DIRECTORY",task.GetWorkingDirectory().c_str(),true);
		
		// Set SSH variables for remote execution if needed by task
		if(!task.GetHost().empty())
		{
			setenv("EVQUEUE_SSH_HOST",task.GetHost().c_str(),true);
		
			if(!task.GetUser().empty())
				setenv("EVQUEUE_SSH_USER",task.GetUser().c_str(),true);
		}
		else if(xmldoc->getDocumentElement()->getAttributes()->getNamedItem(X("host")))
		{
			// Set SSH variables for remote execution if needed by workflow instance
			char *ssh_host;
			ssh_host = XMLString::transcode(xmldoc->getDocumentElement()->getAttribute(X("host")));
			setenv("EVQUEUE_SSH_HOST",ssh_host,true);
			XMLString::release(&ssh_host);
			
			
			if(xmldoc->getDocumentElement()->getAttributes()->getNamedItem(X("user")))
			{
				char *ssh_user;
				ssh_user = XMLString::transcode(xmldoc->getDocumentElement()->getAttribute(X("user")));
				setenv("EVQUEUE_SSH_USER",ssh_user,true);
				XMLString::release(&ssh_user);
			}
		}
		
		if(getenv("EVQUEUE_SSH_HOST"))
		{
			// Set SSH config variables if SSH execution is asked
			setenv("EVQUEUE_SSH_PATH",config->Get("processmanager.monitor.ssh_path").c_str(),true);
			
			if(config->Get("processmanager.monitor.ssh_key").length()>0)
				setenv("EVQUEUE_SSH_KEY",config->Get("processmanager.monitor.ssh_key").c_str(),true);
			
			// Send agent path if needed
			if(task.GetUseAgent())
				setenv("EVQUEUE_SSH_AGENT",config->Get("processmanager.agent.path").c_str(),true);
		}
		
		if(stdin_parameter_c)
		{
			dup2(parameters_pipe[0],STDIN_FILENO);
			close(parameters_pipe[1]);
		}
		
		// Redirect output to log files
		int fno;
		
		fno = open_log_file(tid,LOG_FILENO);
		dup2(fno,LOG_FILENO);
		
		fno = open_log_file(tid,STDOUT_FILENO);
		dup2(fno,STDOUT_FILENO);
		
		if(!task.GetMergeStderr())
			fno = open_log_file(tid,STDERR_FILENO);
		
		dup2(fno,STDERR_FILENO);
		
		if(task.GetParametersMode()==task_parameters_mode::CMDLINE)
		{
			const char *args[parameters_count+4];
			
			args[0] = monitor_path.c_str();
			args[1] = task_filename.c_str();
			args[2] = tid_str;
			
			for(parameters_index=0;parameters_index<parameters_count;parameters_index++)
				args[parameters_index+3] = parameters_value[parameters_index];
			
			args[parameters_index+3] = (char *)0;
			
			execv(monitor_path.c_str(),(char * const *)args);
			
			Logger::Log(LOG_ERR,"Could not execute task monitor");
			tools_send_exit_msg(1,tid,-1);
			exit(-1);
		}
		else if(task.GetParametersMode()==task_parameters_mode::ENV)
		{
			for(parameters_index=0;parameters_index<parameters_count;parameters_index++)
			{
				if(parameters_name[parameters_index])
					setenv(parameters_name[parameters_index],parameters_value[parameters_index],1);
				else
					setenv("DEFAULT_PARAMETER_NAME",parameters_value[parameters_index],1);
			}
			
			execl(monitor_path.c_str(),monitor_path.c_str(),task_filename.c_str(),tid_str,(char *)0);
			
			Logger::Log(LOG_ERR,"Could not execute task monitor");
			tools_send_exit_msg(1,tid,-1);
			exit(-1);
		}
		
		tools_send_exit_msg(1,tid,-1);
		exit(-1);
	}
	
	if(pid>0)
	{
		if(stdin_parameter_c)
		{
			// We have STDIN data
			write(parameters_pipe[1],stdin_parameter_c,strlen(stdin_parameter_c));
			close(parameters_pipe[0]);
			close(parameters_pipe[1]);
			XMLString::release(&stdin_parameter_c);
		}
		
		// Update task node
		((DOMElement *)task_node)->setAttribute(X("status"),X("EXECUTING"));
		
		sprintf(buf,"%d",pid);
		((DOMElement *)task_node)->setAttribute(X("pid"),X(buf));
		
		sprintf(buf,"%d",tid);
		((DOMElement *)task_node)->setAttribute(X("tid"),X(buf));
		
		format_datetime(buf);
		((DOMElement *)task_node)->setAttribute(X("execution_time"),X(buf));
	}
	
	
	if(pid<0)
	{
		Logger::Log(LOG_WARNING,"[ WorkflowInstance ] Unable to execute task : could not fork");
		
		((DOMElement *)task_node)->setAttribute(X("status"),X("ABORTED"));
		((DOMElement *)task_node)->setAttribute(X("error"),X("Unable to fork"));
		
		error_tasks++;
		running_tasks--;
	}
	
	// Clean parameters names and values
	for(parameters_index=0;parameters_index<parameters_count;parameters_index++)
	{
		if(parameters_name[parameters_index])
			XMLString::release(&parameters_name[parameters_index]);
		XMLString::release(&parameters_value[parameters_index]);
	}
	
	delete[] parameters_name;
	delete[] parameters_value;
	
	*workflow_terminated = workflow_ended();
	
	record_savepoint();
	
	pthread_mutex_unlock(&lock);
	
	return pid;
}

bool WorkflowInstance::CheckTaskName(const char *task_name)
{
	int i,len;
	
	len = strlen(task_name);
	if(len>TASK_NAME_MAX_LEN)
		return false;
	
	for(i=0;i<len;i++)
		if(!isalnum(task_name[i]) && task_name[i]!='_')
			return false;
	
	return true;
}

void WorkflowInstance::TaskUpdateProgression(DOMNode *task, int prct)
{
	char buf[16];
	sprintf(buf,"%d",prct);
	((DOMElement *)task)->setAttribute(X("progression"),X(buf));
}

void WorkflowInstance::SendStatus(int s, bool full_status)
{
	pthread_mutex_lock(&lock);
	
	DOMDocument *status_doc;
	
	if(!full_status)
	{
		DOMImplementation *xqillaImplementation = DOMImplementationRegistry::getDOMImplementation(X("XPath2 3.0"));
		status_doc = xqillaImplementation->createDocument();
		
		DOMNode *workflow_node = status_doc->importNode(xmldoc->getDocumentElement(),false);
		status_doc->appendChild(workflow_node);
	}
	else
		status_doc = xmldoc;
	
	XMLCh *status = serializer->writeToString(status_doc->getDocumentElement());
	char *status_c = XMLString::transcode(status);
	
	send(s,status_c,strlen(status_c),0);
	
	XMLString::release(&status);
	XMLString::release(&status_c);
	
	
	if(!full_status)
		status_doc->release();
	
	pthread_mutex_unlock(&lock);
}

void WorkflowInstance::RecordSavepoint()
{
	pthread_mutex_lock(&lock);
	
	record_savepoint(true);
	
	pthread_mutex_unlock(&lock);
}

bool WorkflowInstance::KillTask(pid_t pid)
{
	char buf[256];
	int tasks_index = 0;
	bool found = false;
	
	pthread_mutex_lock(&lock);
	
	// Look for EXECUTING tasks with specified PID
	sprintf(buf,"//task[@status='EXECUTING' and @pid='%d']",pid);
	DOMXPathResult *tasks = xmldoc->evaluate(X(buf),xmldoc->getDocumentElement(),resolver,DOMXPathResult::SNAPSHOT_RESULT_TYPE,0);
	DOMNode *task;
	
	while(tasks->snapshotItem(tasks_index++))
	{
		task = tasks->getNodeValue();
		
		kill(pid,SIGTERM);
		found = true;
	}
	
	tasks->release();
	
	pthread_mutex_unlock(&lock);
	
	return found;
}

void WorkflowInstance::retry_task(DOMNode *task)
{
	unsigned int task_instance_id;
	int retry_delay,retry_times, schedule_level;
	char buf[32];
	char *retry_schedule_c;
	
	// Check if we have a retry_schedule
	const XMLCh *retry_schedule = 0;
	if(task->getAttributes()->getNamedItem(X("retry_schedule")))
		retry_schedule = ((DOMElement *)task)->getAttribute(X("retry_schedule"));
	
	if(task->getAttributes()->getNamedItem(X("retry_schedule_level")))
		schedule_level = XMLString::parseInt(((DOMElement *)task)->getAttribute(X("retry_schedule_level")));
	else
		schedule_level = 0;
	
	if(retry_schedule && schedule_level==0)
	{
		// Retry schedule has not yet begun, so init sequence
		retry_schedule_c = XMLString::transcode(retry_schedule);
		schedule_update(task,retry_schedule_c,&retry_delay,&retry_times);
		XMLString::release(&retry_schedule_c);
	}
	else
	{
		// Check retry parameters (retry_delay and retry_times) allow at least one retry
		if(!task->getAttributes()->getNamedItem(X("retry_delay")))
		{
			error_tasks++; // We won't retry this task, set error
			return;
		}
		
		retry_delay = XMLString::parseInt(task->getAttributes()->getNamedItem(X("retry_delay"))->getNodeValue());
		
		if(!task->getAttributes()->getNamedItem(X("retry_times")))
		{
			error_tasks++; // We won't retry this task, set error
			return;
		}
		
		retry_times = XMLString::parseInt(task->getAttributes()->getNamedItem(X("retry_times"))->getNodeValue());
	}
	
	if(retry_schedule && retry_times==0)
	{
		// We have reached end of the schedule level, update
		retry_schedule_c = XMLString::transcode(retry_schedule);
		schedule_update(task,retry_schedule_c,&retry_delay,&retry_times);
		XMLString::release(&retry_schedule_c);
	}
	
	if(retry_delay==0 || retry_times==0)
	{
		error_tasks++; // No more retry for this task, set error
		return;
	}
	
	// If retry_retval is specified, only retry on specified retval
	if(task->getAttributes()->getNamedItem(X("retry_retval")) && XMLString::parseInt(((DOMElement *)task)->getAttribute(X("retry_retval")))!=XMLString::parseInt(((DOMElement *)task)->getAttribute(X("retval"))))
		return;
	
	// Retry task
	time_t t;
	time(&t);
	t += retry_delay;
	strftime(buf,32,"%Y-%m-%d %H:%M:%S",localtime(&t));
	((DOMElement *)task)->setAttribute(X("retry_at"),X(buf));
	
	sprintf(buf,"%d",retry_times-1);
	((DOMElement *)task)->setAttribute(X("retry_times"),X(buf));
	
	Retrier *retrier = Retrier::GetInstance();
	retrier->InsertTask(this,task,time(0)+retry_delay);
	
	retrying_tasks++;
}

void WorkflowInstance::run(DOMNode *job,DOMNode *context_node)
{
	int jobs_index = 0, matching_nodes_index = 0;
	
	// Loop for tasks
	DOMXPathResult *tasks = xmldoc->evaluate(X("tasks/task"),job,resolver,DOMXPathResult::SNAPSHOT_RESULT_TYPE ,0);
	
	DOMNode *task;
	int tasks_index = 0;
	try
	{
		while(tasks->snapshotItem(tasks_index++))
		{
			task = tasks->getNodeValue();
			
			// Get task status (if present)
			const XMLCh *task_status = 0;
			if(task->getAttributes()->getNamedItem(X("status")))
				task_status = task->getAttributes()->getNamedItem(X("status"))->getNodeValue();
			
			if(task_status && (XMLString::equals(task_status,X("TERMINATED"))))
				continue; // Skip tasks that are already terminated (can happen on resume)
			
			// Check for conditional tasks
			if(task->getAttributes()->getNamedItem(X("condition")))
			{
				DOMXPathResult *test_expr;
				
				try
				{
					// This is unchecked user input, try evaluation
					test_expr = xmldoc->evaluate(task->getAttributes()->getNamedItem(X("condition"))->getNodeValue(),context_node,resolver,DOMXPathResult::FIRST_RESULT_TYPE,0);
				}
				catch(XQillaException &xqe)
				{
					// XPath expression error
					throw Exception("WorkflowInstance","Error while evaluating condition");
				}
				
				int test_value;
				try
				{
					// Xpath expression is correct but evaluation may return no result (e.g. expression refers to inexistant nodes)
					test_value = test_expr->getIntegerValue();
				}
				catch(XQillaException &xqe)
				{
					test_expr->release();
					
					throw Exception("WorkflowInstance","Condition evaluation returned no result");
				}
				
				
				if(!test_value)
				{
					test_expr->release();
					
					((DOMElement *)task)->setAttribute(X("status"),X("SKIPPED"));
					((DOMElement *)task)->setAttribute(X("details"),X("Condition evaluates to false"));
					continue;
				}
				
				test_expr->release();
			}
			
			// Check for looped tasks (must expand them before execution)
			if(task->getAttributes()->getNamedItem(X("loop")))
			{
				const XMLCh *loop_xpath = task->getAttributes()->getNamedItem(X("loop"))->getNodeValue();
				((DOMElement *)task)->removeAttribute(X("loop"));
				
				DOMNode *matching_node;
				DOMXPathResult *matching_nodes;
				
				try
				{
					// This is unchecked user input, try evaluation
					matching_nodes = xmldoc->evaluate(loop_xpath,context_node,resolver,DOMXPathResult::SNAPSHOT_RESULT_TYPE,0);
				}
				catch(XQillaException &xqe)
				{
					((DOMElement *)task)->setAttribute(X("status"),X("ABORTED"));
					((DOMElement *)task)->setAttribute(X("details"),X("Error while evaluating loop"));
					
					// XPath expression error
					throw Exception("WorkflowInstance","Exception in workflow instance");
				}
				
				
				while(matching_nodes->snapshotItem(matching_nodes_index++))
				{
					matching_node = matching_nodes->getNodeValue();
					
					DOMNode *task_clone = task->cloneNode(true);
					task->getParentNode()->appendChild(task_clone);
					
					// Enqueue task
					replace_value(task_clone,matching_node);
					enqueue_task(task_clone);
				}
				
				task->getParentNode()->removeChild(task);
				
				matching_nodes->release();
			}
			else
			{
				// Enqueue task
				replace_value(task,context_node);
				enqueue_task(task);
			}
		}
	}
	catch(Exception &e)
	{
		tasks->release();
		
		throw e;
	}
	
	tasks->release();
}

void WorkflowInstance::run_subjobs(DOMNode *job)
{
	int subjobs_index = 0, matching_nodes_index = 0;
	
	// Loop on subjobs
	DOMXPathResult *subjobs = xmldoc->evaluate(X("subjobs/job"),job,resolver,DOMXPathResult::SNAPSHOT_RESULT_TYPE,0);
	DOMNode *subjob;
	
	try
	{
		while(subjobs->snapshotItem(subjobs_index++))
		{
			subjob = subjobs->getNodeValue();
			
			// Check for conditional jobs
			if(subjob->getAttributes()->getNamedItem(X("condition")))
			{
				DOMXPathResult *test_expr;
				
				try
				{
					// This is unchecked user input, try evaluation
					test_expr = xmldoc->evaluate(subjob->getAttributes()->getNamedItem(X("condition"))->getNodeValue(),job,resolver,DOMXPathResult::FIRST_RESULT_TYPE,0);
				}
				catch(XQillaException &xqe)
				{
					// XPath expression error
					throw Exception("WorkflowInstance","Error while evaluating condition");
				}
				
				int test_value;
				try
				{
					// Xpath expression is correct but evaluation may return no result (e.g. expression refers to inexistant nodes)
					test_value = test_expr->getIntegerValue();
				}
				catch(XQillaException &xqe)
				{
					test_expr->release();
					
					throw Exception("WorkflowInstance","Condition evaluation returned no result");
				}
				
				
				if(!test_value)
				{
					test_expr->release();
					
					((DOMElement *)subjob)->setAttribute(X("status"),X("SKIPPED"));
					((DOMElement *)subjob)->setAttribute(X("details"),X("Condition evaluates to false"));
					continue;
				}
				
				test_expr->release();
			}
			
			// Check for looped jobs (must expand them before execution)
			if(subjob->getAttributes()->getNamedItem(X("loop")))
			{
				const XMLCh *loop_xpath = subjob->getAttributes()->getNamedItem(X("loop"))->getNodeValue();
				((DOMElement *)subjob)->removeAttribute(X("loop"));
				
				DOMNode *matching_node;
				DOMXPathResult *matching_nodes;
				
				try
				{
					// This is unchecked user input, try evaluation
					matching_nodes = xmldoc->evaluate(loop_xpath,job,resolver,DOMXPathResult::SNAPSHOT_RESULT_TYPE,0);
				}
				catch(XQillaException &xqe)
				{
					// XPath expression error
					throw Exception("WorkflowInstance","Error while evaluating loop");
				}
				
				while(matching_nodes->snapshotItem(matching_nodes_index++))
				{
					matching_node = matching_nodes->getNodeValue();
					
					DOMNode *subjob_clone = subjob->cloneNode(true);
					subjob->getParentNode()->appendChild(subjob_clone);
					
					run(subjob_clone,matching_node);
				}
				
				subjob->getParentNode()->removeChild(subjob);
				
				matching_nodes->release();
			}
			else
				run(subjob,job);
		}
	}
	catch(Exception &e)
	{
		// Set error on job
		((DOMElement *)subjob)->setAttribute(X("status"),X("ABORTED"));
		((DOMElement *)subjob)->setAttribute(X("details"),X(e.error));
		
		subjobs->release();
		
		// Terminate workflow
		error_tasks++;
		
		throw e;
	}
	
	subjobs->release();
}


void WorkflowInstance::replace_value(DOMNode *task,DOMNode *context_node)
{
	DOMXPathResult *values;
	DOMNode *value;
	int values_index;
	
	// Replace <value> nodes par their literal value
	values = xmldoc->evaluate(X(".//value"),task,resolver,DOMXPathResult::SNAPSHOT_RESULT_TYPE,0);
	values_index = 0;
	while(values->snapshotItem(values_index++))
	{
		value = values->getNodeValue();
		const XMLCh *xpath_select = value->getAttributes()->getNamedItem(X("select"))->getNodeValue();
		
		DOMXPathResult *value_nodes;
		
		try
		{
			// This is unchecked user input. We have to try evaluation
			value_nodes = xmldoc->evaluate(xpath_select,context_node,resolver,DOMXPathResult::FIRST_RESULT_TYPE,0);
		}
		catch(XQillaException &xqe)
		{
			values->release();
			
			// XPath expression error
			throw Exception("WorkflowInstance","Error computing input values");
		}
		
		DOMNode *value_node;
		DOMNode *old_node;
		if(value_nodes->isNode())
		{
			value_node = value_nodes->getNodeValue();
			old_node = value->getParentNode()->replaceChild(xmldoc->createTextNode(value_node->getTextContent()),value);
			
			old_node->release();
		}
		value_nodes->release();
	}
	
	values->release();
	
	// Replace <copy> nodes par their value
	values = xmldoc->evaluate(X(".//copy"),task,resolver,DOMXPathResult::SNAPSHOT_RESULT_TYPE,0);
	values_index = 0;
	while(values->snapshotItem(values_index++))
	{
		value = values->getNodeValue();
		const XMLCh *xpath_select = value->getAttributes()->getNamedItem(X("select"))->getNodeValue();
		
		DOMXPathResult *result_nodes;
		
		try
		{
			// This is unchecked user input. We have to try evaluation
			result_nodes = xmldoc->evaluate(xpath_select,context_node,resolver,DOMXPathResult::SNAPSHOT_RESULT_TYPE,0);
		}
		catch(XQillaException &xqe)
		{
			values->release();
			
			// XPath expression error
			throw Exception("WorkflowInstance","Error computing input values from copy node");
		}
		
		int result_index = 0;
		DOMNode *result_node;
		while(result_nodes->snapshotItem(result_index++))
		{
			result_node = result_nodes->getNodeValue();
			try
			{
				value->getParentNode()->insertBefore(result_node->cloneNode(true),value);
			}
			catch(DOMException &xe)
			{
				throw Exception("WorkflowInstance","Error computing copy node while replacing values in task input");
			}
		}
		
		result_nodes->release();
		
		value->getParentNode()->removeChild(value);
	}
	
	values->release();
}

void WorkflowInstance::enqueue_task(DOMNode *task)
{
	if(is_cancelling)
	{
		// Workflow is in chancelling state, we won't queue anything more
		((DOMElement *)task)->setAttribute(X("status"),X("ABORTED"));
		((DOMElement *)task)->setAttribute(X("error"),X("Aborted on user request"));
		
		error_tasks++;
		
		return;
	}
	
	// Get task name
	const XMLCh *task_name = task->getAttributes()->getNamedItem(X("name"))->getNodeValue();
	
	// Get queue name (if present) and update task node
	const XMLCh *queue_name = 0;
	char *queue_name_c;

	queue_name = ((DOMElement *)task)->getAttribute(X("queue"));
	queue_name_c = XMLString::transcode(queue_name);
	
	((DOMElement *)task)->setAttribute(X("status"),X("QUEUED"));
	
	QueuePool *pool = QueuePool::GetInstance();
	
	if(!pool->EnqueueTask(queue_name_c,this,task))
	{
		((DOMElement *)task)->setAttribute(X("status"),X("ABORTED"));
		((DOMElement *)task)->setAttribute(X("error"),X("Unknown queue name"));
		
		error_tasks++;
		
		Logger::Log(LOG_WARNING,"[WID %d] Unknown queue name '%s'",workflow_instance_id,queue_name_c);
		
		XMLString::release(&queue_name_c);
		return;
	}
	
	running_tasks++;
	queued_tasks++;
	
	char *task_name_c = XMLString::transcode(task_name);
	Logger::Log(LOG_NOTICE,"[WID %d] Added task %s to queue %s\n",workflow_instance_id,task_name_c,queue_name_c);
	XMLString::release(&task_name_c);
	
	XMLString::release(&queue_name_c);
	
	return;
}

void WorkflowInstance::schedule_update(DOMNode *task,const char *schedule_name,int *retry_delay,int *retry_times)
{
	char buf[256];
	
	// Lookup for specified schedule
	DOMXPathResult *res;
	
	sprintf(buf,"schedules/schedule[@name='%s']",schedule_name);
	res = xmldoc->evaluate(X(buf),xmldoc->getDocumentElement(),resolver,DOMXPathResult::FIRST_RESULT_TYPE,0);
	if(!res->isNode())
	{
		res->release();
		Logger::Log(LOG_WARNING,"[WID %d] Unknown schedule : '%s'",workflow_instance_id,schedule_name);
		return;
	}
	
	DOMNode *schedule= res->getNodeValue();
	res->release();
	
	res = xmldoc->evaluate(X("count(level)"),schedule,resolver,DOMXPathResult::FIRST_RESULT_TYPE,0);
	int schedule_levels = res->getIntegerValue();
	res->release();
	
	// Get current task schedule state
	int current_schedule_level;
	if(task->getAttributes()->getNamedItem(X("retry_schedule_level")))
		current_schedule_level = XMLString::parseInt(((DOMElement *)task)->getAttribute(X("retry_schedule_level")));
	else
		current_schedule_level = 0;
	
	current_schedule_level++;
	sprintf(buf,"%d",current_schedule_level);
	((DOMElement *)task)->setAttribute(X("retry_schedule_level"),X(buf));
	
	if(current_schedule_level>schedule_levels)
	{
		*retry_delay = 0;
		*retry_times = 0;
		return; // We have reached the last schedule level
	}
	
	// Get schedule level details
	sprintf(buf,"level[position()=%d]",current_schedule_level);
	res = xmldoc->evaluate(X(buf),schedule,resolver,DOMXPathResult::FIRST_RESULT_TYPE,0);
	DOMNode *schedule_level = res->getNodeValue();
	res->release();
	
	const XMLCh *retry_delay_str = ((DOMElement *)schedule_level)->getAttribute(X("retry_delay"));
	const XMLCh *retry_times_str = ((DOMElement *)schedule_level)->getAttribute(X("retry_times"));
	
	// Reset task retry informations, according to new shcedule level
	((DOMElement *)task)->setAttribute(X("retry_delay"),retry_delay_str);
	((DOMElement *)task)->setAttribute(X("retry_times"),retry_times_str);
	
	*retry_delay = XMLString::parseInt(retry_delay_str);
	*retry_times = XMLString::parseInt(retry_times_str);
}

bool WorkflowInstance::workflow_ended(void)
{
	if(running_tasks==0 && retrying_tasks==0)
	{
		// End workflow (and notify caller) if no tasks are queued or running at this point
		xmldoc->getDocumentElement()->setAttribute(X("status"),X("TERMINATED"));
		if(error_tasks>0)
		{
			char buf[256];
			sprintf(buf,"%d",error_tasks);
			xmldoc->getDocumentElement()->setAttribute(X("errors"),X(buf));
			
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
	if(XMLString::compareString(X("TERMINATED"),xmldoc->getDocumentElement()->getAttribute(X("status")))==0)
	{
		if(savepoint_level==0)
			return; // Even in forced mode we won't store terminated workflows on level 0
		
		char buf[32];
		format_datetime(buf);
		xmldoc->getDocumentElement()->setAttribute(X("end_time"),X(buf));
		
		sprintf(buf,"%d",error_tasks);
		xmldoc->getDocumentElement()->setAttribute(X("errors"),X(buf));
	}
	else if(!force && savepoint_level<=2)
		return; // On level 1 and 2 we only record savepoints on terminated workflows
	
	XMLCh *savepoint = serializer->writeToString(xmldoc->getDocumentElement());
	char *savepoint_c = XMLString::transcode(savepoint);
	
	// Gather workflow values
	char *workflow_instance_host_c, *workflow_instance_start_c, *workflow_instance_end_c, *workflow_instance_status_c;
	if(xmldoc->getDocumentElement()->hasAttribute(X("host")))
		workflow_instance_host_c = XMLString::transcode(xmldoc->getDocumentElement()->getAttribute(X("host")));
	else
		workflow_instance_host_c = 0;
	
	workflow_instance_start_c = XMLString::transcode(xmldoc->getDocumentElement()->getAttribute(X("start_time")));
	
	if(xmldoc->getDocumentElement()->hasAttribute(X("end_time")))
		workflow_instance_end_c = XMLString::transcode(xmldoc->getDocumentElement()->getAttribute(X("end_time")));
	else
		workflow_instance_end_c = 0;
	
	workflow_instance_status_c = XMLString::transcode(xmldoc->getDocumentElement()->getAttribute(X("status")));
	
	int tries = 0;
	do
	{
		if(tries>0)
		{
			Logger::Log(LOG_NOTICE,"[ WorkflowInstance::record_savepoint() ] Retrying in %d seconds\n",savepoint_retry_wait);
			sleep(savepoint_retry_wait);
		}
		
		try
		{
			DB db;
			
			if(savepoint_level>=2)
			{
				// Workflow has already been insterted into database, just update
				if(XMLString::compareString(X("TERMINATED"),xmldoc->getDocumentElement()->getAttribute(X("status")))!=0)
				{
					// Only update savepoint if workflow is still running
					db.QueryPrintf("UPDATE t_workflow_instance SET workflow_instance_savepoint=%s WHERE workflow_instance_id=%i",savepoint_c,&workflow_instance_id);
				}
				else
				{
					// Update savepoint and status if workflow is terminated
					db.QueryPrintf("UPDATE t_workflow_instance SET workflow_instance_savepoint=%s,workflow_instance_status='TERMINATED',workflow_instance_errors=%i,workflow_instance_end=NOW() WHERE workflow_instance_id=%i",savepoint_c,&error_tasks,&workflow_instance_id);
				}
			}
			else
			{
				// Always insert full informations as we are called at workflow end or when engine restarts
				db.QueryPrintf("\
					INSERT INTO t_workflow_instance(workflow_instance_id,workflow_id,workflow_schedule_id,workflow_instance_host,workflow_instance_start,workflow_instance_end,workflow_instance_status,workflow_instance_errors,workflow_instance_savepoint)\
					VALUES(%i,%i,%i,%s,%s,%s,%s,%i,%s)",
					&workflow_instance_id,&workflow_id,&workflow_schedule_id,workflow_instance_host_c,workflow_instance_start_c,workflow_instance_end_c?workflow_instance_end_c:"0000-00-00 00:00:00",workflow_instance_status_c,&error_tasks,savepoint_c);
			}
			
			break;
		}
		catch(Exception &e)
		{
			Logger::Log(LOG_WARNING,"[ WorkflowInstance::record_savepoint() ] Unexpected exception : [ %s ] %s\n",e.context,e.error);
		}
		
		tries++;
		
	} while(savepoint_retry && (savepoint_retry_times==0 || tries<=savepoint_retry_times));
	
	XMLString::release(&savepoint);
	XMLString::release(&savepoint_c);
	if(workflow_instance_host_c)
		XMLString::release(&workflow_instance_host_c);
	if(workflow_instance_start_c)
		XMLString::release(&workflow_instance_start_c);
	if(workflow_instance_end_c)
		XMLString::release(&workflow_instance_end_c);
	if(workflow_instance_status_c)
		XMLString::release(&workflow_instance_status_c);
}

void WorkflowInstance::format_datetime(char *str)
{
	time_t t;
	struct tm t_desc;
	
	t = time(0);
	localtime_r(&t,&t_desc);
	
	sprintf(str,"%d-%02d-%02d %02d:%02d:%02d",1900+t_desc.tm_year,t_desc.tm_mon+1,t_desc.tm_mday,t_desc.tm_hour,t_desc.tm_min,t_desc.tm_sec);
}

int WorkflowInstance::open_log_file(int tid, int fileno)
{
	char *log_filename = new char[logs_directory.length()+32];
	
	if(fileno==STDOUT_FILENO)
		sprintf(log_filename,"%s/%d.stdout",logs_directory.c_str(),tid);
	else if(fileno==STDERR_FILENO)
		sprintf(log_filename,"%s/%d.stderr",logs_directory.c_str(),tid);
	else
		sprintf(log_filename,"%s/%d.log",logs_directory.c_str(),tid);
	
	int fno;
	fno=open(log_filename,O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR);
	
	delete[] log_filename;
	
	if(fno==-1)
	{
		Logger::Log(LOG_ERR,"Unable to open task output log file (%d)",fileno);
		tools_send_exit_msg(1,tid,-1);
		exit(-1);
	}
	
	return fno;
}

void WorkflowInstance::update_statistics()
{
	char buf[16];
	
	sprintf(buf,"%d",running_tasks);
	xmldoc->getDocumentElement()->setAttribute(X("running_tasks"),X(buf));
	
	sprintf(buf,"%d",queued_tasks);
	xmldoc->getDocumentElement()->setAttribute(X("queued_tasks"),X(buf));
	
	sprintf(buf,"%d",retrying_tasks);
	xmldoc->getDocumentElement()->setAttribute(X("retrying_tasks"),X(buf));
	
	sprintf(buf,"%d",error_tasks);
	xmldoc->getDocumentElement()->setAttribute(X("error_tasks"),X(buf));
}
