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

#include <xqilla/xqilla-dom3.hpp>

using namespace std;
using namespace xercesc;

WorkflowSchedules *WorkflowSchedules::instance = 0;

WorkflowSchedules::WorkflowSchedules()
{
	instance = this;
	
	pthread_mutex_init(&lock, NULL);
	
	Reload();
}

WorkflowSchedules::~WorkflowSchedules()
{
	// Clean current tasks
	for(auto it=schedules_id.begin();it!=schedules_id.end();++it)
		delete it->second;
	
	schedules_id.clear();
	active_schedules.clear();
}

void WorkflowSchedules::Reload(void)
{
	Logger::Log(LOG_NOTICE,"[ WorkflowSchedules ] Reloading workflow schedules definitions");
	
	pthread_mutex_lock(&lock);
	
	// Clean current tasks
	for(auto it=schedules_id.begin();it!=schedules_id.end();++it)
		delete it->second;
	
	schedules_id.clear();
	active_schedules.clear();
	
	DB db;
	
	db.QueryPrintf("SELECT workflow_schedule_id FROM t_workflow_schedule WHERE workflow_schedule_active=1 AND node_name=%s",Configuration::GetInstance()->Get("network.node.name").c_str());
	while(db.FetchRow())
	{
		WorkflowSchedule *workflow_schedule = new WorkflowSchedule(db.GetFieldInt(0));
		
		schedules_id[db.GetFieldInt(0)] = workflow_schedule;
		
		if(workflow_schedule->GetIsActive())
			active_schedules.push_back(workflow_schedule);
	}
	
	pthread_mutex_unlock(&lock);
}

WorkflowSchedule WorkflowSchedules::GetWorkflowSchedule(unsigned int id)
{
	pthread_mutex_lock(&lock);
	
	auto it = schedules_id.find(id);
	if(it==schedules_id.end())
	{
		pthread_mutex_unlock(&lock);
		
		throw Exception("WorkflowSchedules","Unable to find workflow schedule");
	}
	
	WorkflowSchedule workflow_schedule = *it->second;
	
	pthread_mutex_unlock(&lock);
	
	return workflow_schedule;
}

bool WorkflowSchedules::Exists(unsigned int id)
{
	pthread_mutex_lock(&lock);
	
	auto it = schedules_id.find(id);
	if(it==schedules_id.end())
	{
		pthread_mutex_unlock(&lock);
		
		return false;
	}
	
	pthread_mutex_unlock(&lock);
	
	return true;
}

const vector<WorkflowSchedule *> &WorkflowSchedules::GetActiveWorkflowSchedules()
{
	return active_schedules;
}

bool WorkflowSchedules::HandleQuery(SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	WorkflowSchedules *workflow_schedules = WorkflowSchedules::GetInstance();
	
	const std::map<std::string,std::string> attrs = saxh->GetRootAttributes();
	
	auto it_action = attrs.find("action");
	if(it_action==attrs.end())
		return false;
	
	if(it_action->second=="list")
	{
		pthread_mutex_lock(&workflow_schedules->lock);
		
		for(auto it = workflow_schedules->schedules_id.begin(); it!=workflow_schedules->schedules_id.end(); it++)
		{
			WorkflowSchedule workflow_schedule = *it->second;
			DOMElement *node = (DOMElement *)response->AppendXML("<workflow_schedule />");
			node->setAttribute(X("workflow_id"),X(to_string(workflow_schedule.GetWorkflowID()).c_str()));
			node->setAttribute(X("workflow_name"),X(workflow_schedule.GetWorkflowName().c_str()));
			node->setAttribute(X("schedule"),X(workflow_schedule.GetScheduleDescription().c_str()));
			node->setAttribute(X("onfailure"),X(workflow_schedule.GetOnFailureBehavior()==CONTINUE?"CONTINUE":"SUSPEND"));
			node->setAttribute(X("user"),X(workflow_schedule.GetUser().c_str()));
			node->setAttribute(X("host"),X(workflow_schedule.GetHost().c_str()));
			node->setAttribute(X("active"),X(workflow_schedule.GetIsActive()?"1":"0"));
			node->setAttribute(X("comment"),X(workflow_schedule.GetComment().c_str()));
		}
		
		pthread_mutex_unlock(&workflow_schedules->lock);
		
		return true;
	}
	
	return false;
}