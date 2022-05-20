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

#include <Workflow/Workflows.h>
#include <Workflow/Workflow.h>
#include <DB/DB.h>
#include <Exception/Exception.h>
#include <Logger/Logger.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <Cluster/Cluster.h>
#include <Crypto/Sha1String.h>
#include <User/User.h>
#include <API/QueryHandlers.h>

#include <string.h>

Workflows *Workflows::instance = 0;

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	qh->RegisterHandler("workflows",Workflows::HandleQuery);
	qh->RegisterReloadHandler("workflows", Workflows::HandleReload);
	return (APIAutoInit *)0;
});

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
	
	unique_lock<mutex> llock(lock);
	
	clear();
	
	// Update
	DB db;
	DB db2(&db);
	db.Query("SELECT workflow_id,workflow_name FROM t_workflow");
	
	while(db.FetchRow())
		add(db.GetFieldInt(0),db.GetField(1),new Workflow(&db2,db.GetField(1)));
	
	llock.unlock();
	
	if(notify)
	{
		// Notify cluster
		Cluster::GetInstance()->Notify("<control action='reload' module='workflows' notify='no' />\n");
	}
}

bool Workflows::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	Workflows *workflows = Workflows::GetInstance();
	
	string action = query->GetRootAttribute("action");
	
	if(action=="list")
	{
		bool full = query->GetRootAttributeBool("full",false);
		
		unique_lock<mutex> llock(workflows->lock);
		
		for(auto it = workflows->objects_name.begin(); it!=workflows->objects_name.end(); it++)
		{
			Workflow workflow = *it->second;
			
			if(!user.HasAccessToWorkflow(workflow.GetID(), "read"))
				continue;
			
			DOMElement node;
			if(full)
				node = (DOMElement)response->AppendXML(workflow.GetXML());
			else
				node = (DOMElement)response->AppendXML("<workflow/>");
			
			node.setAttribute("id",to_string(workflow.GetID()));
			node.setAttribute("name",workflow.GetName());
			node.setAttribute("group",workflow.GetGroup());
			node.setAttribute("comment",workflow.GetComment());
			node.setAttribute("lastcommit",workflow.GetLastCommit());
			node.setAttribute("modified",workflow.GetIsModified()?"1":"0");
		}
		
		return true;
	}
	
	return false;
}

void Workflows::HandleReload(bool notify)
{
	Workflows *workflows = Workflows::GetInstance();
	workflows->Reload(notify);
}

void Workflows::HandleNotificationTypeDelete(unsigned int id)
{
	DB db;
	DB db2(&db);
	
	db.QueryPrintf("SELECT notification_id FROM t_notification WHERE notification_type_id=%i", {&id});
	while(db.FetchRow())
	{
		int notification_id = db.GetFieldInt(0);
		
		// Ensure no workflows are bound to removed notifications
		db2.QueryPrintf("DELETE FROM t_workflow_notification WHERE notification_id=%i", {&notification_id});
	}
}
