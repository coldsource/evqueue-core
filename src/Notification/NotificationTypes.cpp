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

#include <Notification/NotificationTypes.h>
#include <Notification/NotificationType.h>
#include <DB/DB.h>
#include <Exception/Exception.h>
#include <Logger/Logger.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <Cluster/Cluster.h>
#include <User/User.h>
#include <Crypto/sha1.h>

#include <string.h>

NotificationTypes *NotificationTypes::instance = 0;

using namespace std;

NotificationTypes::NotificationTypes():APIObjectList()
{
	instance = this;
	
	Reload(false);
}

NotificationTypes::~NotificationTypes()
{
}

void NotificationTypes::Reload(bool notify)
{
	Logger::Log(LOG_NOTICE,"Reloading configuration from database");
	
	unique_lock<mutex> llock(lock);
	
	clear();
	
	// Update
	DB db;
	DB db2(&db);
	db.Query("SELECT notification_type_id,notification_type_name FROM t_notification_type");
	
	while(db.FetchRow())
		add(db.GetFieldInt(0),db.GetField(1),new NotificationType(&db2,db.GetFieldInt(0)));
	
	llock.unlock();
	
	if(notify)
	{
		// Notify cluster
		Cluster::GetInstance()->Notify("<control action='reload' module='notifications' notify='no' />\n");
	}
}

void NotificationTypes::SyncBinaries(bool notify)
{
	Logger::Log(LOG_NOTICE,"[ NotificationTypes ] Syncing binaries");
	
	unique_lock<mutex> llock(lock);
	
	DB db;
	
	// Load tasks binaries from database
	db.Query("SELECT notification_type_id, notification_type_name, notification_type_binary_content FROM t_notification_type WHERE notification_type_binary_content IS NOT NULL");
	
	struct sha1_ctx ctx;
	char db_hash[20];
	string file_hash;
	
	while(db.FetchRow())
	{
		// Compute database SHA1 hash
		sha1_init_ctx(&ctx);
		sha1_process_bytes(db.GetField(2).c_str(),db.GetFieldLength(2),&ctx);
		sha1_finish_ctx(&ctx,db_hash);
		
		// Compute file SHA1 hash
		try
		{
			NotificationType::GetFileHash(db.GetField(0),file_hash);
		}
		catch(Exception &e)
		{
			Logger::Log(LOG_NOTICE,"[ NotificationTypes ] Task "+db.GetField(1)+" was not found creating it");
			
			NotificationType::PutFile(db.GetField(0),string(db.GetField(2).c_str(),db.GetFieldLength(2)),false);
			continue;
		}
		
		if(memcmp(file_hash.c_str(),db_hash,20)==0)
		{
			Logger::Log(LOG_NOTICE,"[ NotificationTypes ] Task "+db.GetField(1)+" hash matches DB, skipping");
			continue;
		}
		
		Logger::Log(LOG_NOTICE,"[ NotificationTypes ] Task "+db.GetField(1)+" hash does not match DB, replacing");
		
		NotificationType::PutFile(db.GetField(0),string(db.GetField(2).c_str(),db.GetFieldLength(2)),false);
	}
	
	llock.unlock();
	
	if(notify)
	{
		// Notify cluster
		Cluster::GetInstance()->Notify("<control action='syncnotifications' notify='no' />\n");
	}
}

bool NotificationTypes::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	NotificationTypes *notification_types = NotificationTypes::GetInstance();
	
	const string action = query->GetRootAttribute("action");
	
	if(action=="list")
	{
		unique_lock<mutex> llock(notification_types->lock);
		
		for(auto it = notification_types->objects_name.begin(); it!=notification_types->objects_name.end(); it++)
		{
			NotificationType notification_type = *it->second;
			DOMElement node = (DOMElement)response->AppendXML("<notification_type />");
			node.setAttribute("id",to_string(notification_type.GetID()));
			node.setAttribute("name",notification_type.GetName());
			node.setAttribute("description",notification_type.GetDescription());
			node.setAttribute("binary",notification_type.GetBinary());
		}
		
		return true;
	}
	
	return false;
}
