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

#include <string.h>

#include <xqilla/xqilla-dom3.hpp>

Workflows *Workflows::instance = 0;

using namespace std;
using namespace xercesc;

Workflows::Workflows()
{
	instance = this;
	
	pthread_mutex_init(&lock, NULL);
	
	Reload();
}

Workflows::~Workflows()
{
	// Clean current tasks
	std::map<std::string,Workflow *>::iterator it;
	for(it=workflows.begin();it!=workflows.end();++it)
		delete it->second;
	
	workflows.clear();
}

void Workflows::Reload(void)
{
	Logger::Log(LOG_NOTICE,"[ Workflows ] Reloading workflows definitions");
	
	pthread_mutex_lock(&lock);
	
	// Clean current tasks
	std::map<std::string,Workflow *>::iterator it;
	for(it=workflows.begin();it!=workflows.end();++it)
		delete it->second;
	
	workflows.clear();
	
	// Update
	DB db;
	DB db2(&db);
	db.Query("SELECT workflow_name FROM t_workflow");
	
	while(db.FetchRow())
	{
		std::string Workflow_name(db.GetField(0));
		workflows[Workflow_name] = new Workflow(&db2,db.GetField(0));
	}
	
	pthread_mutex_unlock(&lock);
}

Workflow Workflows::GetWorkflow(const string &name)
{
	pthread_mutex_lock(&lock);
	
	std::map<std::string,Workflow *>::iterator it;
	it = workflows.find(name);
	if(it==workflows.end())
	{
		pthread_mutex_unlock(&lock);
		
		throw Exception("Workflows","Unable to find workflow");
	}
	
	Workflow workflow = *it->second;
	
	pthread_mutex_unlock(&lock);
	
	return workflow;
}

bool Workflows::HandleQuery(SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	Workflows *workflows = Workflows::GetInstance();
	
	const std::map<std::string,std::string> attrs = saxh->GetRootAttributes();
	
	auto it_action = attrs.find("action");
	if(it_action==attrs.end())
		return false;
	
	if(it_action->second=="list")
	{
		pthread_mutex_lock(&workflows->lock);
		
		for(auto it = workflows->workflows.begin(); it!=workflows->workflows.end(); it++)
		{
			Workflow workflow = *it->second;
			DOMElement *node = (DOMElement *)response->AppendXML(workflow.GetXML());
			node->setAttribute(X("name"),X(workflow.GetName()));
		}
		
		pthread_mutex_unlock(&workflows->lock);
		
		return true;
	}
	
	return false;
}
