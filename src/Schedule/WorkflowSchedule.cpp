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

#include <Schedule/WorkflowSchedule.h>
#include <Schedule/WorkflowSchedules.h>
#include <Schedule/WorkflowScheduler.h>
#include <Workflow/Workflow.h>
#include <Workflow/Workflows.h>
#include <Exception/Exception.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <Configuration/ConfigurationEvQueue.h>
#include <DB/DB.h>
#include <Logger/Logger.h>
#include <Logger/LoggerAPI.h>
#include <User/User.h>
#include <WS/Events.h>
#include <API/QueryHandlers.h>

#include <string.h>

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	qh->RegisterHandler("workflow_schedule",WorkflowSchedule::HandleQuery);
	Events::GetInstance()->RegisterEvents({"WORKFLOWSCHEDULE_CREATED","WORKFLOWSCHEDULE_MODIFIED","WORKFLOWSCHEDULE_REMOVED"});
	return (APIAutoInit *)0;
});

using namespace std;

WorkflowSchedule::WorkflowSchedule(unsigned int workflow_schedule_id)
{
	this->workflow_schedule_id = workflow_schedule_id;
	
	DB db;
	
	// Fetch schedule parameters
	db.QueryPrintf("SELECT workflow_id, workflow_schedule, workflow_schedule_onfailure,workflow_schedule_host,workflow_schedule_user,workflow_schedule_active, workflow_schedule_comment, node_name FROM t_workflow_schedule WHERE workflow_schedule_id=%i",{&workflow_schedule_id});
	
	if(!db.FetchRow())
		throw Exception("WorkflowSchedule","Unknown schedule ID");
	
	Logger::Log(LOG_DEBUG,"[WSID "+to_string(workflow_schedule_id)+"] Loading workflow schedule for workflow '"+db.GetField(0)+"'");
	
	this->workflow_id = db.GetFieldInt(0);
	
	schedule = Schedule(db.GetField(1));
	schedule_description = db.GetField(1);
	
	if(db.GetField(2)=="CONTINUE")
		onfailure = CONTINUE;
	else if(db.GetField(2)=="SUSPEND")
		onfailure = SUSPEND;
	
	if(!db.GetFieldIsNULL(3))
		workflow_schedule_host = db.GetField(3);
	
	if(!db.GetFieldIsNULL(4))
		workflow_schedule_user = db.GetField(4);
	
	active = db.GetFieldInt(5);
	comment = db.GetField(6);
	node = db.GetField(7);
	
	// Fetch schedule parameters
	db.QueryPrintf("SELECT workflow_schedule_parameter,workflow_schedule_parameter_value FROM t_workflow_schedule_parameters WHERE workflow_schedule_id=%i",{&workflow_schedule_id});
	while(db.FetchRow())
		parameters.Add(db.GetField(0),db.GetField(1));
}

WorkflowSchedule::~WorkflowSchedule()
{
}

const string WorkflowSchedule::GetWorkflowName(void) const
{
	Workflow workflow = Workflows::GetInstance()->Get(workflow_id);
	return workflow.GetName();
}

void WorkflowSchedule::SetStatus(bool active)
{
	DB db;
	
	if(active)
		db.QueryPrintf("UPDATE t_workflow_schedule SET workflow_schedule_active=1 WHERE workflow_schedule_id=%i",{&workflow_schedule_id});
	else
		db.QueryPrintf("UPDATE t_workflow_schedule SET workflow_schedule_active=0 WHERE workflow_schedule_id=%i",{&workflow_schedule_id});
}

void WorkflowSchedule::Get(unsigned int id, QueryResponse *response)
{
	WorkflowSchedule workflow_schedule = WorkflowSchedules::GetInstance()->Get(id);
	
	DOMElement node = (DOMElement)response->AppendXML("<workflow_schedule />");
	node.setAttribute("workflow_id",to_string(workflow_schedule.GetWorkflowID()));
	node.setAttribute("schedule",workflow_schedule.GetScheduleDescription());
	node.setAttribute("onfailure",workflow_schedule.GetOnFailureBehavior()==CONTINUE?"CONTINUE":"SUSPEND");
	node.setAttribute("user",workflow_schedule.GetUser());
	node.setAttribute("host",workflow_schedule.GetHost());
	node.setAttribute("active",workflow_schedule.GetIsActive()?"1":"0");
	node.setAttribute("comment",workflow_schedule.GetComment());
	node.setAttribute("node",workflow_schedule.GetNode());
	
	string parameter_name;
	string parameter_value;
	WorkflowParameters *parameters = workflow_schedule.GetParameters();
	
	parameters->SeekStart();
	while(parameters->Get(parameter_name,parameter_value))
	{
		DOMElement parameter_node = response->GetDOM()->createElement("parameter");
		parameter_node.setAttribute("name",parameter_name);
		parameter_node.setAttribute("value",parameter_value);
		
		node.appendChild(parameter_node);
	}
}

unsigned int WorkflowSchedule::Create(
	unsigned int workflow_id,
	const string &node,
	const string &schedule_description,
	bool onfailure_continue,
	const string &user,
	const string &host,
	bool active,
	const string &comment,
	WorkflowParameters *parameters
)
{
	create_edit_check(workflow_id, node, schedule_description, onfailure_continue, user, host, active, comment, parameters);
	
	int iactive = active;
	
	DB db;
	
	db.StartTransaction();
	
	string onfailure_continue_str = onfailure_continue?"CONTINUE":"SUSPEND";
	
	db.QueryPrintf(
		"INSERT INTO t_workflow_schedule(node_name,workflow_id,workflow_schedule,workflow_schedule_onfailure,workflow_schedule_user,workflow_schedule_host,workflow_schedule_active,workflow_schedule_comment) VALUES(%s,%i,%s,%s,%s,%s,%i,%s)", {
		&node,
		&workflow_id,
		&schedule_description,
		&onfailure_continue_str,
		user.length()?&user:0,
		host.length()?&host:0,
		&iactive,
		&comment
	});
	
	unsigned int workflow_schedule_id = db.InsertID();
	
	string parameter_name;
	string parameter_value;
	
	parameters->SeekStart();
	while(parameters->Get(parameter_name,parameter_value))
		db.QueryPrintf(
			"INSERT INTO t_workflow_schedule_parameters(workflow_schedule_id,workflow_schedule_parameter,workflow_schedule_parameter_value) VALUES(%i,%s,%s)",  {
			&workflow_schedule_id,
			&parameter_name,
			&parameter_value
		});
	
	db.CommitTransaction();
	
	return workflow_schedule_id;
}

void WorkflowSchedule::Edit(
	unsigned int id,
	unsigned int workflow_id,
	const string &node,
	const string &schedule_description,
	bool onfailure_continue,
	const string &user,
	const string &host,
	bool active,
	const string &comment,
	WorkflowParameters *parameters
)
{
	create_edit_check(workflow_id, node, schedule_description, onfailure_continue, user, host, active, comment, parameters);
	
	int iactive = active;
	string sonfailure_continue = onfailure_continue?"CONTINUE":"SUSPEND";
	
	DB db;
	
	db.StartTransaction();
	
	// We only store locally schedules that belong to our node, we have to check existence against DB
	if(!WorkflowSchedules::GetInstance()->Exists(id))
		throw Exception("WorkflowSchedule","Workflow schedule not found","UNKNOWN_WORKFLOW_SCHEDULE");
	
	db.QueryPrintf(
		"UPDATE t_workflow_schedule SET node_name=%s,workflow_id=%i,workflow_schedule=%s,workflow_schedule_onfailure=%s,workflow_schedule_user=%s,workflow_schedule_host=%s,workflow_schedule_active=%i,workflow_schedule_comment=%s WHERE workflow_schedule_id=%i",  {
		&node,
		&workflow_id,
		&schedule_description,
		&sonfailure_continue,
		user.length()?&user:0,
		host.length()?&host:0,
		&iactive,
		&comment,
		&id
	});
	
	string parameter_name;
	string parameter_value;
	
	db.QueryPrintf("DELETE FROM t_workflow_schedule_parameters WHERE workflow_schedule_id=%i",{&id});
	
	parameters->SeekStart();
	while(parameters->Get(parameter_name,parameter_value))
		db.QueryPrintf(
			"INSERT INTO t_workflow_schedule_parameters(workflow_schedule_id,workflow_schedule_parameter,workflow_schedule_parameter_value) VALUES(%i,%s,%s)", {
			&id,
			&parameter_name,
			&parameter_value
		});
	
	db.CommitTransaction();
}

void WorkflowSchedule::SetIsActive(unsigned int id,bool active)
{
	int iactive = active;
	
	DB db;
	
	// We only store locally schedules that belong to our node, we have to check existence against DB
	if(!WorkflowSchedules::GetInstance()->Exists(id))
		throw Exception("WorkflowSchedule","Workflow schedule not found","UNKNOWN_WORKFLOW_SCHEDULE");
	
	db.QueryPrintf("UPDATE t_workflow_schedule SET workflow_schedule_active=%i WHERE workflow_schedule_id=%i",{&iactive,&id});
}

void WorkflowSchedule::Delete(unsigned int id)
{
	DB db;
	
	db.StartTransaction();
	
	db.QueryPrintf("DELETE FROM t_workflow_schedule WHERE workflow_schedule_id=%i",{&id});
	
	if(db.AffectedRows()==0)
		throw Exception("WorkflowSchedule","Workflow schedule not found","UNKNOWN_WORKFLOW_SCHEDULE");
	
	db.QueryPrintf("DELETE FROM t_workflow_schedule_parameters WHERE workflow_schedule_id=%i", {&id});
	
	db.CommitTransaction();
}

void WorkflowSchedule::create_edit_check(
	unsigned int workflow_id,
	const string &node,
	const string &schedule_description,
	bool onfailure,
	const string &user,
	const string &host,
	bool active,
	const string &comment,
	WorkflowParameters *parameters
)
{
	// Check schedule is valid by trying to instantiate it
	Schedule schedule(schedule_description);
	
	// Check workflow ID exists
	Workflow workflow = Workflows::GetInstance()->Get(workflow_id);
	
	// Check corectness of input parameters
	workflow.CheckInputParameters(parameters);
}

bool WorkflowSchedule::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	const string action = query->GetRootAttribute("action");
	
	if(action=="get")
	{
		unsigned int id = query->GetRootAttributeInt("id");
		
		Get(id,response);
		
		return true;
	}
	else if(action=="create" || action=="edit")
	{
		unsigned int workflow_id = query->GetRootAttributeInt("workflow_id");
		string node = query->GetRootAttribute("node");
		string schedule = query->GetRootAttribute("schedule");
		
		string onfailure = query->GetRootAttribute("onfailure");
		bool onfailure_continue;
		if(onfailure=="CONTINUE")
			onfailure_continue = true;
		else if(onfailure=="SUSPEND")
			onfailure_continue = false;
		else
			throw Exception("WorkflowSchedule","onfailure must be 'CONTINUE' or 'SUSPEND'","INVALID_PARAMETER");
		
		string remote_user = query->GetRootAttribute("user","");
		string remote_host = query->GetRootAttribute("host","");
		bool active = query->GetRootAttributeBool("active",true);
		string comment =  query->GetRootAttribute("comment","");
		unsigned int id;
		
		string ev;
		
		if(action=="create")
		{
			id = Create(workflow_id,node,schedule,onfailure_continue,remote_user,remote_host,active,comment,query->GetWorkflowParameters());
			
			LoggerAPI::LogAction(user,id,"WorkflowSchedule",query->GetQueryGroup(),action);
			
			ev = "WORKFLOWSCHEDULE_CREATED";
			
			response->GetDOM()->getDocumentElement().setAttribute("schedule-id",to_string(id));
		}
		else
		{
			id = query->GetRootAttributeInt("id");
			
			Edit(id, workflow_id,node,schedule,onfailure_continue,remote_user,remote_host,active,comment,query->GetWorkflowParameters());
			
			ev = "WORKFLOWSCHEDULE_MODIFIED";
			
			LoggerAPI::LogAction(user,id,"WorkflowSchedule",query->GetQueryGroup(),action);
		}
		
		WorkflowScheduler::GetInstance()->Reload();
		
		Events::GetInstance()->Create(ev,id);
		
		return true;
	}
	else if(action=="delete")
	{
		unsigned int id = query->GetRootAttributeInt("id");
		
		Delete(id);
		
		LoggerAPI::LogAction(user,id,"WorkflowSchedule",query->GetQueryGroup(),action);
		
		Events::GetInstance()->Create("WORKFLOWSCHEDULE_REMOVED",id);
		
		WorkflowScheduler::GetInstance()->Reload();
		
		return true;
	}
	else if(action=="lock")
	{
		unsigned int id = query->GetRootAttributeInt("id");
		
		SetIsActive(id,false);
		
		LoggerAPI::LogAction(user,id,"WorkflowSchedule",query->GetQueryGroup(),action);
		
		WorkflowScheduler::GetInstance()->Reload();
		
		Events::GetInstance()->Create("WORKFLOWSCHEDULE_MODIFIED",id);
		
		return true;
	}
	else if(action=="unlock")
	{
		unsigned int id = query->GetRootAttributeInt("id");
		
		SetIsActive(id,true);
		
		LoggerAPI::LogAction(user,id,"WorkflowSchedule",query->GetQueryGroup(),action);
		
		WorkflowScheduler::GetInstance()->Reload();
		
		Events::GetInstance()->Create("WORKFLOWSCHEDULE_MODIFIED",id);
		
		return true;
	}
	
	return false;
}
