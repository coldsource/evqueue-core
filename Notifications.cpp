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

#include <string.h>

Notifications *Notifications::instance = 0;

using namespace std;

Notifications::Notifications()
{
	instance = this;
	
	pthread_mutex_init(&lock, NULL);
	
	max_concurrency = Configuration::GetInstance()->GetInt("notifications.tasks.concurrency");
	
	Reload();
}

Notifications::~Notifications()
{
	// Clean current notifications
	map<unsigned int,Notification *>::iterator it;
	for(it=notifications.begin();it!=notifications.end();++it)
		delete it->second;
	
	notifications.clear();
}

void Notifications::Reload(void)
{
	Logger::Log(LOG_NOTICE,"[ Notifications ] Reloading notifications definitions");
	
	pthread_mutex_lock(&lock);
	
	// Clean current notifications
	map<unsigned int,Notification *>::iterator it;
	for(it=notifications.begin();it!=notifications.end();++it)
		delete it->second;
	
	notifications.clear();
	
	// Update
	DB db;
	DB db2(&db);
	db.Query("SELECT notification_id FROM t_notification");
	
	while(db.FetchRow())
	{
		notifications[db.GetFieldInt(0)] = new Notification(&db2,db.GetFieldInt(0));
	}
	
	pthread_mutex_unlock(&lock);
}

Notification Notifications::GetNotification(unsigned int id)
{
	pthread_mutex_lock(&lock);
	
	map<unsigned int,Notification *>::iterator it;
	it = notifications.find(id);
	if(it==notifications.end())
	{
		pthread_mutex_unlock(&lock);
		
		throw Exception("Notifications","Unable to find notification");
	}
	
	Notification notification = *it->second;
	
	pthread_mutex_unlock(&lock);
	
	return notification;
}

void Notifications::Call(unsigned int notification_id, WorkflowInstance *workflow_instance)
{	
	try
	{
		Notification notification = GetNotification(notification_id);
		
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
		Logger::Log(LOG_ERR,"Exception during notifications processing : %s",e.error);
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