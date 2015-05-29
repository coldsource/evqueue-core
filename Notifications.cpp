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
#include <WorkflowInstance.h>

#include <string.h>

Notifications *Notifications::instance = 0;

Notifications::Notifications()
{
	instance = this;
	
	pthread_mutex_init(&lock, NULL);
	
	Reload();
}

Notifications::~Notifications()
{
	// Clean current notifications
	std::map<unsigned int,Notification *>::iterator it;
	for(it=notifications.begin();it!=notifications.end();++it)
		delete it->second;
	
	notifications.clear();
}

void Notifications::Reload(void)
{
	Logger::Log(LOG_NOTICE,"[ Notifications ] Reloading notifications definitions");
	
	pthread_mutex_lock(&lock);
	
	// Clean current notifications
	std::map<unsigned int,Notification *>::iterator it;
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
	
	std::map<unsigned int,Notification *>::iterator it;
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
		notification.Call(workflow_instance);
	}
	catch(Exception &e)
	{
		Logger::Log(LOG_ERR,"Exception during notifications processing : %s",e.error);
	}
}