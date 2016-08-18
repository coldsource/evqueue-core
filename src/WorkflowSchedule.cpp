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

#include <WorkflowSchedule.h>
#include <WorkflowSchedules.h>
#include <WorkflowScheduler.h>
#include <Workflow.h>
#include <Workflows.h>
#include <Exception.h>
#include <SocketQuerySAX2Handler.h>
#include <QueryResponse.h>
#include <Configuration.h>
#include <DB.h>
#include <Logger.h>

#include <string.h>

#include <xqilla/xqilla-dom3.hpp>

using namespace std;
using namespace xercesc;

WorkflowSchedule::WorkflowSchedule(unsigned int workflow_schedule_id)
{
	this->workflow_schedule_id = workflow_schedule_id;
	
	DB db;
	
	// Fetch schedule parameters
	db.QueryPrintf("SELECT workflow_id, workflow_schedule, workflow_schedule_onfailure,workflow_schedule_host,workflow_schedule_user,workflow_schedule_active, workflow_schedule_comment FROM t_workflow_schedule WHERE workflow_schedule_id=%i",&workflow_schedule_id);
	
	if(!db.FetchRow())
		throw Exception("WorkflowSchedule","Unknown schedule ID");
	
	Logger::Log(LOG_NOTICE,"[WSID %d] Loading workflow schedule for workflow '%s'",workflow_schedule_id,db.GetField(0));
	
	this->workflow_id = db.GetFieldInt(0);
	
	schedule = Schedule(db.GetField(1));
	
	if(strcmp(db.GetField(2),"CONTINUE")==0)
		onfailure = CONTINUE;
	else if(strcmp(db.GetField(2),"SUSPEND")==0)
		onfailure = SUSPEND;
	
	if(db.GetField(3))
		workflow_schedule_host = db.GetField(3);
	
	if(db.GetField(4))
		workflow_schedule_user = db.GetField(4);
	
	active = db.GetFieldInt(5);
	comment = db.GetField(6);
	
	// Fetch schedule parameters
	db.QueryPrintf("SELECT workflow_schedule_parameter,workflow_schedule_parameter_value FROM t_workflow_schedule_parameters WHERE workflow_schedule_id=%i",&workflow_schedule_id);
	while(db.FetchRow())
		parameters.Add(db.GetField(0),db.GetField(1));
}

WorkflowSchedule::~WorkflowSchedule()
{
}

const string WorkflowSchedule::GetWorkflowName(void)
{
	Workflow workflow = Workflows::GetInstance()->Get(workflow_id);
	return workflow.GetName();
}

void WorkflowSchedule::SetStatus(bool active)
{
	DB db;
	
	if(active)
		db.QueryPrintf("UPDATE t_workflow_schedule SET workflow_schedule_active=1 WHERE workflow_schedule_id=%i",&workflow_schedule_id);
	else
		db.QueryPrintf("UPDATE t_workflow_schedule SET workflow_schedule_active=0 WHERE workflow_schedule_id=%i",&workflow_schedule_id);
}

void WorkflowSchedule::Get(unsigned int id, QueryResponse *response)
{
	WorkflowSchedule workflow_schedule = WorkflowSchedules::GetInstance()->Get(id);
	
	DOMElement *node = (DOMElement *)response->AppendXML("<workflow_schedule />");
	node->setAttribute(X("workflow_id"),X(to_string(workflow_schedule.GetWorkflowID()).c_str()));
	node->setAttribute(X("schedule"),X(workflow_schedule.GetScheduleDescription().c_str()));
	node->setAttribute(X("onfailure"),X(workflow_schedule.GetOnFailureBehavior()==CONTINUE?"CONTINUE":"SUSPEND"));
	node->setAttribute(X("user"),X(workflow_schedule.GetUser().c_str()));
	node->setAttribute(X("host"),X(workflow_schedule.GetHost().c_str()));
	node->setAttribute(X("active"),X(workflow_schedule.GetIsActive()?"1":"0"));
	node->setAttribute(X("comment"),X(workflow_schedule.GetComment().c_str()));
	
	string parameter_name;
	string parameter_value;
	WorkflowParameters *parameters = workflow_schedule.GetParameters();
	
	parameters->SeekStart();
	while(parameters->Get(parameter_name,parameter_value))
	{
		DOMElement *parameter_node = response->GetDOM()->createElement(X("parameter"));
		parameter_node->setAttribute(X("name"),X(parameter_name.c_str()));
		parameter_node->setAttribute(X("value"),X(parameter_value.c_str()));
		
		node->appendChild(parameter_node);
	}
}

void WorkflowSchedule::Create(
	unsigned int workflow_id,
	const string &schedule_description,
	bool onfailure_continue,
	const string &user,
	const string &host,
	bool active,
	const string &comment,
	WorkflowParameters *parameters
)
{
	create_edit_check(workflow_id, schedule_description, onfailure_continue, user, host, active, comment, parameters);
	
	int iactive = active;
	
	DB db;
	
	db.StartTransaction();
	
	string onfailure_continue_str = onfailure_continue?"CONTINUE":"SUSPEND";
	
	db.QueryPrintf(
		"INSERT INTO t_workflow_schedule(node_name,workflow_id,workflow_schedule,workflow_schedule_onfailure,workflow_schedule_user,workflow_schedule_host,workflow_schedule_active,workflow_schedule_comment) VALUES(%s,%i,%s,%s,%s,%s,%i,%s)",
		&Configuration::GetInstance()->Get("network.node.name"),
		&workflow_id,
		&schedule_description,
		&onfailure_continue_str,
		user.length()?&user:0,
		host.length()?&host:0,
		&iactive,
		&comment
		);
	
	unsigned int workflow_schedule_id = db.InsertID();
	
	string parameter_name;
	string parameter_value;
	
	parameters->SeekStart();
	while(parameters->Get(parameter_name,parameter_value))
		db.QueryPrintf(
			"INSERT INTO t_workflow_schedule_parameters(workflow_schedule_id,workflow_schedule_parameter,workflow_schedule_parameter_value) VALUES(%i,%s,%s)",
			&workflow_schedule_id,
			&parameter_name,
			&parameter_value
		);
	
	db.CommitTransaction();
}

void WorkflowSchedule::Edit(
	unsigned int id,
	unsigned int workflow_id,
	const string &schedule_description,
	bool onfailure_continue,
	const string &user,
	const string &host,
	bool active,
	const string &comment,
	WorkflowParameters *parameters
)
{
	create_edit_check(workflow_id, schedule_description, onfailure_continue, user, host, active, comment, parameters);
	
	if(!WorkflowSchedules::GetInstance()->Exists(id))
		throw Exception("WorkflowSchedule","Workflow schedule not found");
	
	int iactive = active;
	
	DB db;
	
	db.StartTransaction();
	
	db.QueryPrintf(
		"UPDATE t_workflow_schedule SET node_name=%s,workflow_id=%i,workflow_schedule=%s,workflow_schedule_onfailure=%s,workflow_schedule_user=%s,workflow_schedule_host=%s,workflow_schedule_active=%i,workflow_schedule_comment=%s WHERE workflow_schedule_id=%i",
		&Configuration::GetInstance()->Get("network.node.name"),
		&workflow_id,
		&schedule_description,
		onfailure_continue?"CONTINUE":"SUSPEND",
		user.length()?&user:0,
		host.length()?&host:0,
		&iactive,
		&comment,
		&id
		);
	
	unsigned int workflow_schedule_id = db.InsertID();
	
	string parameter_name;
	string parameter_value;
	
	db.QueryPrintf("DELETE FROM t_workflow_schedule_parameters WHERE workflow_schedule_id=%i",&id);
	
	parameters->SeekStart();
	while(parameters->Get(parameter_name,parameter_value))
		db.QueryPrintf(
			"INSERT INTO t_workflow_schedule_parameters(workflow_schedule_id,workflow_schedule_parameter,workflow_schedule_parameter_value) VALUES(%i,%s,%s)",
			&workflow_schedule_id,
			&parameter_name,
			&parameter_value
		);
	
	db.CommitTransaction();
}

void WorkflowSchedule::Delete(unsigned int id)
{
	DB db;
	
	db.StartTransaction();
	
	db.QueryPrintf("DELETE FROM t_workflow_schedule WHERE workflow_schedule_id=%i",&id);
	
	if(db.AffectedRows()==0)
		throw Exception("WorkflowSchedule","Workflow schedule not found");
	
	db.QueryPrintf("DELETE FROM t_workflow_schedule_parameters WHERE workflow_schedule_id=%i",&id);
	
	db.CommitTransaction();
}

void WorkflowSchedule::create_edit_check(
	unsigned int workflow_id,
	const string &schedule_description,
	bool onfailure,
	const string &user,
	const string &host,
	bool active,
	const string &comment,
	WorkflowParameters *parameters
)
{
	// Check schedule is valid by trying to instanciate it
	Schedule schedule(schedule_description.c_str());
	
	// Check workflow ID exists
	Workflow workflow = Workflows::GetInstance()->Get(workflow_id);
	
	// Check corectness of input parameters
	workflow.CheckInputParameters(parameters);
}

bool WorkflowSchedule::HandleQuery(SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	const string action = saxh->GetRootAttribute("action");
	
	if(action=="get")
	{
		unsigned int id = saxh->GetRootAttributeInt("id");
		
		Get(id,response);
		
		return true;
	}
	else if(action=="create" || action=="edit")
	{
		unsigned int workflow_id = saxh->GetRootAttributeInt("workflow_id");
		string schedule = saxh->GetRootAttribute("schedule");
		
		string onfailure = saxh->GetRootAttribute("onfailure");
		bool onfailure_continue;
		if(onfailure=="CONTINUE")
			onfailure_continue = true;
		else if(onfailure=="SUSPEND")
			onfailure_continue = false;
		else
			throw Exception("WorkflowSchedule","onfailure must be 'CONTINUE' or 'SUSPEND'");
		
		string user = saxh->GetRootAttribute("user","");
		string host = saxh->GetRootAttribute("host","");
		bool active = saxh->GetRootAttributeBool("active",true);
		string comment =  saxh->GetRootAttribute("comment","");
		
		if(action=="create")
			Create(workflow_id,schedule,onfailure_continue,user,host,active,comment,saxh->GetWorkflowParameters());
		else
		{
			unsigned int id = saxh->GetRootAttributeInt("id");
			
			Edit(id, workflow_id,schedule,onfailure_continue,user,host,active,comment,saxh->GetWorkflowParameters());
		}
		
		WorkflowScheduler::GetInstance()->Reload();
		
		return true;
	}
	else if(action=="delete")
	{
		unsigned int id = saxh->GetRootAttributeInt("id");
		
		Delete(id);
		
		WorkflowScheduler::GetInstance()->Reload();
		
		return true;
	}
	
	return false;
}