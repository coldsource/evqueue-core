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

#include <Notifications.h>
#include <Notification.h>
#include <DB.h>
#include <Exception.h>
#include <Logger.h>
#include <Configuration.h>
#include <WorkflowInstance.h>
#include <SocketQuerySAX2Handler.h>
#include <QueryResponse.h>

#include <string.h>

#include <xqilla/xqilla-dom3.hpp>

Notifications *Notifications::instance = 0;

using namespace std;
using namespace xercesc;

Notifications::Notifications():APIObjectList()
{
	instance = this;
	
	max_concurrency = Configuration::GetInstance()->GetInt("notifications.tasks.concurrency");
	
	Reload();
}

Notifications::~Notifications()
{
}

void Notifications::Reload(void)
{
	Logger::Log(LOG_NOTICE,"[ Notifications ] Reloading notifications definitions");
	
	pthread_mutex_lock(&lock);
	
	clear();
	
	// Update
	DB db;
	DB db2(&db);
	db.Query("SELECT notification_id FROM t_notification");
	
	while(db.FetchRow())
		add(db.GetFieldInt(0),"",new Notification(&db2,db.GetFieldInt(0)));
	
	pthread_mutex_unlock(&lock);
}

void Notifications::Call(unsigned int notification_id, WorkflowInstance *workflow_instance)
{	
	try
	{
		Notification notification = Get(notification_id);
		
		pthread_mutex_lock(&lock);
		
		if(notification_instances.size()<max_concurrency)
		{
			pid_t pid = notification.Call(workflow_instance);
			if(pid>0)
				notification_instances.insert(pair<pid_t,st_notification_instance>(pid,{workflow_instance->GetInstanceID(),notification.GetName()}));
		}
		else
		{
			Logger::Log(LOG_WARNING,"Maximum concurrency reached for notifications calls, dropping call for notification '%s' of workflow instance %d",notification.GetName().c_str(),workflow_instance->GetInstanceID());
		}
		
		pthread_mutex_unlock(&lock);
	}
	catch(Exception &e)
	{
		Logger::Log(LOG_ERR,"Exception during notifications processing : %s",e.error.c_str());
	}
}

void Notifications::Exit(pid_t pid, int status, char retcode)
{
	pthread_mutex_lock(&lock);
	
	map<pid_t,st_notification_instance>::iterator it;
	it = notification_instances.find(pid);
	if(it==notification_instances.end())
	{
		pthread_mutex_unlock(&lock);
		Logger::Log(LOG_WARNING,"[ Notifications ] Got exit from pid %d but could not find corresponding notification",pid);
		return;
	}
	
	st_notification_instance ni = it->second;
	
	if(status==0)
	{
		if(retcode!=0)
			Logger::Log(LOG_WARNING,"Notification task '%s' (pid %d) for workflow instance %d returned code %d",ni.notification_type.c_str(),pid,ni.workflow_instance_id,retcode);
		else
			Logger::Log(LOG_NOTICE,"Notification task '%s' (pid %d) for workflow instance %d executed successuflly",ni.notification_type.c_str(),pid,ni.workflow_instance_id);
	}
	else if(status==1)
		Logger::Log(LOG_WARNING,"Notification task '%s' (pid %d) for workflow instance %d was killed",ni.notification_type.c_str(),pid,ni.workflow_instance_id);
	else if(status==2)
		Logger::Log(LOG_WARNING,"Notification task '%s' (pid %d) for workflow instance %d timed out",ni.notification_type.c_str(),pid,ni.workflow_instance_id);
	else if(status==3)
		Logger::Log(LOG_ALERT,"Notification task '%s' (pid %d) for workflow instance %d could not be forked",ni.notification_type.c_str(),pid,ni.workflow_instance_id);
	
	notification_instances.erase(pid);
	
	pthread_mutex_unlock(&lock);
}

bool Notifications::HandleQuery(SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	Notifications *notifications = Notifications::GetInstance();
	
	const string action = saxh->GetRootAttribute("action");
	
	if(action=="list")
	{
		pthread_mutex_lock(&notifications->lock);
		
		for(auto it = notifications->objects_id.begin(); it!=notifications->objects_id.end(); it++)
		{
			DOMElement *node = (DOMElement *)response->AppendXML("<notification />");
			node->setAttribute(X("id"),X(std::to_string(it->second->GetID()).c_str()));
			node->setAttribute(X("type_id"),X(std::to_string(it->second->GetTypeID()).c_str()));
			node->setAttribute(X("name"),X(it->second->GetName().c_str()));
		}
		
		pthread_mutex_unlock(&notifications->lock);
		
		return true;
	}
	
	return false;
}