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
#include <Workflow/WorkflowParameters.h>
#include <Schedule/WorkflowSchedule.h>
#include <Schedule/WorkflowScheduler.h>
#include <Workflow/Workflows.h>
#include <Workflow/Workflow.h>
#include <XPath/WorkflowXPathFunctions.h>
#include <DB/DB.h>
#include <Exception/Exception.h>
#include <WorkflowInstance/ExceptionWorkflowContext.h>
#include <API/Statistics.h>
#include <Configuration/ConfigurationEvQueue.h>
#include <WorkflowInstance/WorkflowInstances.h>
#include <Logger/Logger.h>
#include <Schedule/RetrySchedules.h>
#include <Schedule/RetrySchedule.h>
#include <DB/SequenceGenerator.h>
#include <Notification/Notifications.h>
#include <API/QueryResponse.h>
#include <WS/Events.h>
#include <nlohmann/json.hpp>
#include <global.h>

#include <memory>

using namespace std;
using nlohmann::json;

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
		db.QueryPrintf("INSERT INTO t_workflow_instance(node_name,workflow_id,workflow_schedule_id,workflow_instance_host,workflow_instance_status,workflow_instance_start,workflow_instance_comment, workflow_instance_savepoint, workflow_instance_errors) VALUES(%s,%i,%i,%s,'EXECUTING',NOW(),%s,'',0)",&ConfigurationEvQueue::GetInstance()->Get("cluster.node.name"),&workflow_id,workflow_schedule_id?&workflow_schedule_id:0,&workflow_host,&workflow_comment);
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
	
	xmldoc->getDocumentElement().setAttribute("id",to_string(workflow_instance_id));
	
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
		json j;
		j["instance"] = GetDOM()->Serialize(GetDOM()->getDocumentElement());
		
		for(int i = 0; i < notifications.size(); i++)
		{
			Notifications::GetInstance()->Call(
				notifications.at(i),
				"workflow instance "+to_string(workflow_instance_id),
				{to_string(workflow_instance_id), to_string(error_tasks), ConfigurationEvQueue::GetInstance()->Get("network.bind.path")},
				j
			);
		}

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
	
	Events::GetInstance()->Create("INSTANCE_TERMINATED",workflow_instance_id);
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
