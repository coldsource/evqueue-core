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

#include <WorkflowSchedules.h>
#include <WorkflowSchedule.h>
#include <DB.h>
#include <Logger.h>
#include <Configuration.h>
#include <Exception.h>
#include <SocketQuerySAX2Handler.h>
#include <QueryResponse.h>
#include <User.h>

#include <xqilla/xqilla-dom3.hpp>

using namespace std;
using namespace xercesc;

WorkflowSchedules *WorkflowSchedules::instance = 0;

WorkflowSchedules::WorkflowSchedules():APIObjectList()
{
	instance = this;
	
	Reload(false);
}

WorkflowSchedules::~WorkflowSchedules()
{
}

void WorkflowSchedules::Reload(bool notify)
{
	Logger::Log(LOG_NOTICE,"Reloading workflow schedules definitions");
	
	pthread_mutex_lock(&lock);
	
	clear();
	active_schedules.clear();
	
	DB db;
	
	db.QueryPrintf("SELECT workflow_schedule_id FROM t_workflow_schedule WHERE workflow_schedule_active=1 AND node_name=%s",&Configuration::GetInstance()->Get("network.node.name"));
	while(db.FetchRow())
	{
		WorkflowSchedule *workflow_schedule = new WorkflowSchedule(db.GetFieldInt(0));
		
		add(db.GetFieldInt(0),"",workflow_schedule);
		
		if(workflow_schedule->GetIsActive())
			active_schedules.push_back(workflow_schedule);
	}
	
	pthread_mutex_unlock(&lock);
}

const vector<WorkflowSchedule *> &WorkflowSchedules::GetActiveWorkflowSchedules()
{
	return active_schedules;
}

bool WorkflowSchedules::HandleQuery(const User &user, SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	WorkflowSchedules *workflow_schedules = WorkflowSchedules::GetInstance();
	
	string action = saxh->GetRootAttribute("action");
	
	if(action=="list")
	{
		DB db;
		db.Query("SELECT ws.workflow_schedule_id, ws.node_name, ws.workflow_id, ws.workflow_schedule, ws.workflow_schedule_onfailure, ws.workflow_schedule_user, ws.workflow_schedule_host, ws.workflow_schedule_active, ws.workflow_schedule_comment, w.workflow_name FROM t_workflow_schedule ws, t_workflow w WHERE ws.workflow_id=w.workflow_id");
		
		while(db.FetchRow())
		{
			DOMElement *node = (DOMElement *)response->AppendXML("<workflow_schedule />");
			node->setAttribute(X("id"),X(db.GetField(0)));
			node->setAttribute(X("node"),X(db.GetField(1)));
			node->setAttribute(X("workflow_id"),X(db.GetField(2)));
			node->setAttribute(X("schedule"),X(db.GetField(3)));
			node->setAttribute(X("onfailure"),X(db.GetField(4)));
			node->setAttribute(X("user"),X(db.GetField(5)));
			node->setAttribute(X("host"),X(db.GetField(6)));
			node->setAttribute(X("active"),X(db.GetField(7)));
			node->setAttribute(X("comment"),X(db.GetField(8)));
			node->setAttribute(X("workflow_name"),X(db.GetField(9)));
		}
		
		return true;
	}
	
	return false;
}