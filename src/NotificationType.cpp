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

#include <xqilla/xqilla-dom3.hpp>

using namespace std;
using namespace xercesc;

NotificationType::NotificationType(DB *db,unsigned int notification_type_id)
{
	db->QueryPrintf("SELECT notification_type_id,notification_type_name,notification_type_description FROM t_notification_type WHERE notification_type_id=%i",&notification_type_id);
	
	if(!db->FetchRow())
		throw Exception("NotificationType","Unknown notification type");
	
	id = db->GetFieldInt(0);
	name = db->GetField(1);
	description = db->GetField(2);
}

void NotificationType::PutFile(const string &filename,const string &data,bool base64_encoded)
{
	if(base64_encoded)
		FileManager::PutFile(Configuration::GetInstance()->Get("notifications.tasks.directory"),filename,data,FileManager::FILETYPE_BINARY,FileManager::DATATYPE_BASE64);
	else
		FileManager::PutFile(Configuration::GetInstance()->Get("notifications.tasks.directory"),filename,data,FileManager::FILETYPE_BINARY,FileManager::DATATYPE_BINARY);
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

void NotificationType::PutConfFile(const string &filename,const string &data,bool base64_encoded)
{
	if(base64_encoded)
		FileManager::PutFile(Configuration::GetInstance()->Get("notifications.tasks.directory")+"/conf",filename,data,FileManager::FILETYPE_CONF,FileManager::DATATYPE_BASE64);
	else
		FileManager::PutFile(Configuration::GetInstance()->Get("notifications.tasks.directory")+"/conf",filename,data,FileManager::FILETYPE_CONF,FileManager::DATATYPE_BINARY);
}

void NotificationType::GetConfFileHash(const string &filename,string &hash)
{
	FileManager::GetFileHash(Configuration::GetInstance()->Get("notifications.tasks.directory")+"/conf",filename,hash);
}

void NotificationType::GetConfFile(const string &filename,string &data)
{
	FileManager::GetFile(Configuration::GetInstance()->Get("notifications.tasks.directory")+"/conf",filename,data);
}

void NotificationType::RemoveConfFile(const string &filename)
{
	FileManager::RemoveFile(Configuration::GetInstance()->Get("notifications.tasks.directory")+"/conf",filename);
}

void NotificationType::Get(unsigned int id, QueryResponse *response)
{
	NotificationType type = NotificationTypes::GetInstance()->Get(id);
	
	DOMElement *node = (DOMElement *)response->AppendXML("<notification_type />");
	node->setAttribute(X("name"),X(type.GetName().c_str()));
	node->setAttribute(X("description"),X(type.GetDescription().c_str()));
}

void NotificationType::Register(const std::string &name, const std::string &description, const std::string binary_content)
{
	if(name.length()==0)
		throw Exception("NotificationType","Name cannot be empty");
	
	DB db;
	db.QueryPrintf(
		"INSERT INTO t_notification_type(notification_type_name,notification_type_description,notification_type_binary_content) VALUES(%s,%s,%s)",
		&name,
		&description,
		binary_content.length()?&binary_content:0
		);
}

void NotificationType::Unregister(unsigned int id)
{
	DB db;
	
	NotificationType notification_type = NotificationTypes::GetInstance()->Get(id);
	
	db.StartTransaction();
	
	db.QueryPrintf("DELETE FROM t_notification_type WHERE notification_type_id=%i",&id);
	
	db.QueryPrintf("DELETE FROM t_notification WHERE notification_type_id=%i",&id);
	
	// Remove binary
	try
	{
		RemoveFile(notification_type.name);
	}
	catch(Exception &e) {}
	
	// Remove config file
	try
	{
		RemoveConfFile(notification_type.name);
	}
	catch(Exception &e) {}	
	
	db.CommitTransaction();
}

void NotificationType::GetConf(unsigned int id, QueryResponse *response)
{
	DB db;
	db.QueryPrintf("SELECT notification_type_conf_content FROM t_notification_type WHERE notification_type_id=%i",&id);
	
	if(!db.FetchRow())
		throw Exception("NotificationType","Unable to find notification type");
	
	string conf = db.GetField(0)?db.GetField(0):"";
	string conf_base64;
	base64_encode_string(conf,conf_base64);
	
	DOMElement *node = (DOMElement *)response->AppendXML("<conf>");
	node->setAttribute(X("content"),X(conf_base64.c_str()));
}

void NotificationType::SetConf(unsigned int id, const string &data)
{
	if(!NotificationTypes::GetInstance()->Exists(id))
		throw Exception("NotificationType","Unable to find notification type");
	
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
		string binary_content_base64 = saxh->GetRootAttribute("binary_content","");
		string binary_content;
		if(binary_content_base64.length())
			base64_decode_string(binary_content,binary_content_base64);
		
		Register(name,description,binary_content);
		
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
		
		NotificationTypes::GetInstance()->SyncBinaries();
		
		return true;
	}
	
	return false;
}