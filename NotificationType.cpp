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

using namespace std;

NotificationType::NotificationType(DB *db,unsigned int notification_type_id)
{
	db->QueryPrintf("SELECT notification_type_id,notification_type_name,notification_type_description,notification_type_binary FROM t_notification_type WHERE notification_type_id=%i",&notification_type_id);
	
	if(!db->FetchRow())
		throw Exception("NotificationType","Unknown notification type");
	
	id = db->GetFieldInt(0);
	name = db->GetField(1);
	description = db->GetField(2);
	binary = db->GetField(3);
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

void NotificationType::Register(const std::string &name, const std::string &description, const std::string &binary, const std::string binary_content)
{
	if(name.length()==0)
		throw Exception("NotificationType","Name cannot be empty");
	
	if(binary.length()==0)
		throw Exception("NotificationType","binary is invalid");
	
	DB db;
	db.QueryPrintf(
		"INSERT INTO t_notification_type(notification_type_name,notification_type_description,notification_type_binary,notification_type_binary_content) VALUES(%s,%s,%s,%s)",
		name.c_str(),
		description.c_str(),
		binary.c_str(),
		binary_content.length()?binary_content.c_str():0
		);
}

void NotificationType::Unregister(unsigned int id)
{
	DB db;
	db.QueryPrintf("DELETE FROM t_notification_type WHERE notification_type_id=%i",&id);
	
	if(db.AffectedRows()==0)
		throw Exception("NotificationType","Unable to find notification type");
}

bool NotificationType::HandleQuery(SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	const string action = saxh->GetRootAttribute("action");
	
	if(action=="register")
	{
		string name = saxh->GetRootAttribute("name");
		string description = saxh->GetRootAttribute("description");
		string binary = saxh->GetRootAttribute("binary");
		string binary_content_base64 = saxh->GetRootAttribute("binary_content","");
		string binary_content;
		if(binary_content_base64.length())
			base64_decode_string(binary_content,binary_content_base64);
		
		Register(name,description,binary,binary_content);
		
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
	
	return false;
}