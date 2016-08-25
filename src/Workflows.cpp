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

#include <string.h>

#include <xqilla/xqilla-dom3.hpp>

Workflows *Workflows::instance = 0;

using namespace std;
using namespace xercesc;

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
	Logger::Log(LOG_NOTICE,"[ Workflows ] Reloading workflows definitions");
	
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

bool Workflows::HandleQuery(SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	Workflows *workflows = Workflows::GetInstance();
	
	string action = saxh->GetRootAttribute("action");
	
	if(action=="list")
	{
		pthread_mutex_lock(&workflows->lock);
		
		for(auto it = workflows->objects_name.begin(); it!=workflows->objects_name.end(); it++)
		{
			Workflow workflow = *it->second;
			DOMElement *node = (DOMElement *)response->AppendXML(workflow.GetXML());
			node->setAttribute(X("id"),X(std::to_string(workflow.GetID()).c_str()));
			node->setAttribute(X("name"),X(workflow.GetName().c_str()));
			node->setAttribute(X("group"),X(workflow.GetGroup().c_str()));
			node->setAttribute(X("comment"),X(workflow.GetComment().c_str()));
			node->setAttribute(X("bound-to-schedule"),workflow.GetIsBoundSchedule()?X("1"):X("0"));
			node->setAttribute(X("has-bound-task"),workflow.GetIsBoundTask()?X("1"):X("0"));
			node->setAttribute(X("lastcommit"),X(workflow.GetLastCommit().c_str()));
		}
		
		pthread_mutex_unlock(&workflows->lock);
		
		return true;
	}
	
	return false;
}
