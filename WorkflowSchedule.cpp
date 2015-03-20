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
#include <Exception.h>
#include <DB.h>
#include <Logger.h>

#include <string.h>

WorkflowSchedule::WorkflowSchedule(unsigned int workflow_schedule_id)
{
	this->workflow_schedule_id = workflow_schedule_id;
	
	DB db;
	
	// Fetch schedule parameters
	db.QueryPrintf("SELECT w.workflow_name, ws.workflow_schedule, ws.workflow_schedule_onfailure,workflow_schedule_host,workflow_schedule_user FROM t_workflow_schedule ws, t_workflow w WHERE ws.workflow_id=w.workflow_id AND ws.workflow_schedule_id=%i",&workflow_schedule_id);
	
	if(!db.FetchRow())
		throw Exception("WorkflowSchedule","Unknown schedule ID");
	
	Logger::Log(LOG_NOTICE,"[WSID %d] Loading workflow schedule for workflow '%s'",workflow_schedule_id,db.GetField(0));
	
	this->workflow_name = new char[strlen(db.GetField(0))+1];
	strcpy(this->workflow_name,db.GetField(0));
	
	schedule = new Schedule(db.GetField(1));
	
	if(strcmp(db.GetField(2),"CONTINUE")==0)
		onfailure = CONTINUE;
	else if(strcmp(db.GetField(2),"SUSPEND")==0)
		onfailure = SUSPEND;
	
	if(db.GetField(3))
	{
		this->workflow_schedule_host = new char[strlen(db.GetField(3))+1];
		strcpy(this->workflow_schedule_host,db.GetField(3));
	}
	else
		this->workflow_schedule_host = 0;
	
	if(db.GetField(4))
	{
		this->workflow_schedule_user = new char[strlen(db.GetField(4))+1];
		strcpy(this->workflow_schedule_user,db.GetField(4));
	}
	else
		this->workflow_schedule_user = 0;
	
	// Fetch schedule parameters
	db.QueryPrintf("SELECT workflow_schedule_parameter,workflow_schedule_parameter_value FROM t_workflow_schedule_parameters WHERE workflow_schedule_id=%i",&workflow_schedule_id);
	while(db.FetchRow())
		parameters.Add(db.GetField(0),db.GetField(1));
}

WorkflowSchedule::~WorkflowSchedule()
{
	delete[] workflow_name;
	if(workflow_schedule_host)
		delete[] workflow_schedule_host;
	if(workflow_schedule_user)
		delete[] workflow_schedule_user;
	delete schedule;
}

void WorkflowSchedule::SetStatus(bool active)
{
	DB db;
	
	if(active)
		db.QueryPrintf("UPDATE t_workflow_schedule SET workflow_schedule_active=1 WHERE workflow_schedule_id=%i",&workflow_schedule_id);
	else
		db.QueryPrintf("UPDATE t_workflow_schedule SET workflow_schedule_active=0 WHERE workflow_schedule_id=%i",&workflow_schedule_id);
}

