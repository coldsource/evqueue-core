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

#include <NotificationType.h>
#include <NotificationTypes.h>
#include <Notifications.h>
#include <Workflows.h>
#include <Exception.h>
#include <SocketQuerySAX2Handler.h>
#include <QueryResponse.h>
#include <FileManager.h>
#include <Configuration.h>
#include <base64.h>
#include <DB.h>
#include <User.h>

using namespace std;

NotificationType::NotificationType(DB *db,unsigned int notification_type_id)
{
	db->QueryPrintf("SELECT notification_type_id,notification_type_name,notification_type_description,notification_type_manifest,notification_type_conf_content FROM t_notification_type WHERE notification_type_id=%i",&notification_type_id);
	
	if(!db->FetchRow())
		throw Exception("NotificationType","Unknown notification type");
	
	id = db->GetFieldInt(0);
	name = db->GetField(1);
	description = db->GetField(2);
	manifest = db->GetField(3);
	configuration = db->GetField(4);
}

void NotificationType::PutFile(const string &filename,const string &data,bool base64_encoded)
{
	if(base64_encoded)
		FileManager::PutFile(Configuration::GetInstance()->Get("notifications.tasks.directory"),filename,data,FileManager::FILETYPE_CONF,FileManager::DATATYPE_BASE64);
	else
		FileManager::PutFile(Configuration::GetInstance()->Get("notifications.tasks.directory"),filename,data,FileManager::FILETYPE_CONF,FileManager::DATATYPE_BINARY);
	
	FileManager::Chmod(Configuration::GetInstance()->Get("notifications.tasks.directory"),filename,S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
}

void NotificationType::GetFile(const string &filename,string &data)
{
	FileManager::GetFile(Configuration::GetInstance()->Get("notifications.tasks.directory"),filename,data);
}

void NotificationType::GetFileHash(const string &filename,string &hash)
{
	FileManager::GetFileHash(Configuration::GetInstance()->Get("notifications.tasks.directory"),filename,hash);
}

void NotificationType::RemoveFile(const string &filename)
{
	FileManager::RemoveFile(Configuration::GetInstance()->Get("notifications.tasks.directory"),filename);
}

void NotificationType::Get(unsigned int id, QueryResponse *response)
{
	NotificationType type = NotificationTypes::GetInstance()->Get(id);
	
	DOMElement node = (DOMElement)response->AppendXML("<notification_type />");
	node.setAttribute("name",type.GetName());
	node.setAttribute("description",type.GetDescription());
	response->AppendXML(type.GetManifest());
}

void NotificationType::Register(const string &name, const string &description, const string &manifest, const string &binary_content)
{
	if(name.length()==0)
		throw Exception("NotificationType","Name cannot be empty","INVALID_PARAMETER");
	
	DB db;
	db.QueryPrintf(
		"INSERT INTO t_notification_type(notification_type_name,notification_type_description,notification_type_manifest,notification_type_binary_content) VALUES(%s,%s,%s,%s) ON DUPLICATE KEY UPDATE notification_type_description=VALUES(notification_type_description),notification_type_manifest=VALUES(notification_type_manifest),notification_type_binary_content=VALUES(notification_type_binary_content)",
		&name,
		&description,
		&manifest,
		binary_content.length()?&binary_content:0
		);
}

void NotificationType::Unregister(unsigned int id)
{
	DB db;
	
	NotificationType notification_type = NotificationTypes::GetInstance()->Get(id);
	
	db.StartTransaction();
	
	// Delete notification type
	db.QueryPrintf("DELETE FROM t_notification_type WHERE notification_type_id=%i",&id);
	
	// Remove binary
	try
	{
		RemoveFile(notification_type.name);
	}
	catch(Exception &e) {}
	
	// Delete associated notifications
	db.QueryPrintf("DELETE FROM t_notification WHERE notification_type_id=%i",&id);
	
	// Ensure no workflows are bound to removed notifications
	db.Query("DELETE FROM t_workflow_notification WHERE NOT EXISTS(SELECT * FROM t_notification n WHERE t_workflow_notification.notification_id=n.notification_id)");
	
	db.CommitTransaction();
}

void NotificationType::GetConf(unsigned int id, QueryResponse *response)
{
	DB db;
	db.QueryPrintf("SELECT notification_type_conf_content FROM t_notification_type WHERE notification_type_id=%i",&id);
	
	if(!db.FetchRow())
		throw Exception("NotificationType","Unable to find notification type","UNKNOWN_NOTIFICATION_TYPE");
	
	string conf = db.GetField(0);
	string conf_base64;
	base64_encode_string(conf,conf_base64);
	
	DOMElement node = (DOMElement)response->AppendXML("<conf>");
	node.setAttribute("content",conf_base64);
}

void NotificationType::SetConf(unsigned int id, const string &data)
{
	if(!NotificationTypes::GetInstance()->Exists(id))
		throw Exception("NotificationType","Unable to find notification type","UNKNOWN_NOTIFICATION_TYPE");
	
	DB db;
	db.QueryPrintf("UPDATE t_notification_type SET notification_type_conf_content=%s WHERE notification_type_id=%i",&data,&id);
}

bool NotificationType::HandleQuery(const User &user, SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	const string action = saxh->GetRootAttribute("action");
	
	if(action=="get")
	{
		unsigned int id = saxh->GetRootAttributeInt("id");
		
		Get(id, response);
		
		return true;
	}
	else if(action=="register")
	{
		string name = saxh->GetRootAttribute("name");
		string description = saxh->GetRootAttribute("description");
		string manifest_base64 = saxh->GetRootAttribute("manifest","");
		string manifest;
		if(manifest_base64.length())
			base64_decode_string(manifest,manifest_base64);
		string binary_content_base64 = saxh->GetRootAttribute("binary_content","");
		string binary_content;
		if(binary_content_base64.length())
			base64_decode_string(binary_content,binary_content_base64);
		
		Register(name,description,manifest,binary_content);
		
		NotificationTypes::GetInstance()->Reload();
		NotificationTypes::GetInstance()->SyncBinaries();
		Notifications::GetInstance()->Reload();
		Workflows::GetInstance()->Reload();
		
		return true;
	}
	else if(action=="unregister")
	{
		unsigned int id = saxh->GetRootAttributeInt("id");
		
		Unregister(id);
		
		NotificationTypes::GetInstance()->Reload();
		NotificationTypes::GetInstance()->SyncBinaries();
		Notifications::GetInstance()->Reload();
		Workflows::GetInstance()->Reload();
		
		return true;
	}
	else if(action=="get_conf")
	{
		unsigned int id = saxh->GetRootAttributeInt("id");
		
		GetConf(id, response);
		
		return true;
	}
	else if(action=="set_conf")
	{
		unsigned int id = saxh->GetRootAttributeInt("id");
		string content_base64 = saxh->GetRootAttribute("content","");
		string content;
		if(content_base64.length())
			base64_decode_string(content,content_base64);
		
		SetConf(id, content);
		
		NotificationTypes::GetInstance()->Reload();
		Notifications::GetInstance()->Reload();
		
		return true;
	}
	
	return false;
}
