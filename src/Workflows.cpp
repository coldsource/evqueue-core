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

#include <Workflows.h>
#include <Workflow.h>
#include <DB.h>
#include <Exception.h>
#include <Logger.h>
#include <SocketQuerySAX2Handler.h>
#include <QueryResponse.h>
#include <Cluster.h>
#include <Sha1String.h>
#include <User.h>

#include <string.h>

Workflows *Workflows::instance = 0;

using namespace std;

Workflows::Workflows():APIObjectList()
{
	instance = this;
	
	Reload(false);
}

Workflows::~Workflows()
{
}

void Workflows::Reload(bool notify)
{
	Logger::Log(LOG_NOTICE,"Reloading workflows definitions");
	
	pthread_mutex_lock(&lock);
	
	clear();
	
	// Update
	DB db;
	DB db2(&db);
	db.Query("SELECT workflow_id,workflow_name FROM t_workflow");
	
	while(db.FetchRow())
		add(db.GetFieldInt(0),db.GetField(1),new Workflow(&db2,db.GetField(1)));
	pthread_mutex_unlock(&lock);
	
	if(notify)
	{
		// Notify cluster
		Cluster::GetInstance()->ExecuteCommand("<control action='reload' module='workflows' notify='no' />\n");
	}
}

bool Workflows::HandleQuery(const User &user, SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	Workflows *workflows = Workflows::GetInstance();
	
	string action = saxh->GetRootAttribute("action");
	
	if(action=="list")
	{
		pthread_mutex_lock(&workflows->lock);
		
		for(auto it = workflows->objects_name.begin(); it!=workflows->objects_name.end(); it++)
		{
			Workflow workflow = *it->second;
			
			if(!user.HasAccessToWorkflow(workflow.GetID(), "read"))
				continue;
			
			DOMElement node = (DOMElement)response->AppendXML(workflow.GetXML());
			node.setAttribute("id",to_string(workflow.GetID()));
			node.setAttribute("name",workflow.GetName());
			node.setAttribute("group",workflow.GetGroup());
			node.setAttribute("comment",workflow.GetComment());
			node.setAttribute("bound-to-schedule",workflow.GetIsBoundSchedule()?"1":"0");
			node.setAttribute("has-bound-task",workflow.GetIsBoundTask()?"1":"0");
			node.setAttribute("bound-task-id",to_string(workflow.GetBoundTaskID()));
			node.setAttribute("lastcommit",workflow.GetLastCommit());
			node.setAttribute("modified",workflow.GetIsModified()?"1":"0");
		}
		
		pthread_mutex_unlock(&workflows->lock);
		
		return true;
	}
	
	return false;
}
