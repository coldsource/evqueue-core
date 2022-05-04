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

#include <Notification/NotificationType.h>
#include <Notification/NotificationTypes.h>
#include <Notification/Notifications.h>
#include <Workflow/Workflows.h>
#include <Exception/Exception.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <IO/FileManager.h>
#include <Configuration/ConfigurationEvQueue.h>
#include <Crypto/base64.h>
#include <DB/DB.h>
#include <User/User.h>
#include <Zip/Zip.h>
#include <DOM/DOMDocument.h>
#include <WS/Events.h>

#include <memory>

using namespace std;

NotificationType::NotificationType(DB *db,unsigned int notification_type_id)
{
	db->QueryPrintf("SELECT notification_type_id,notification_type_scope,notification_type_name,notification_type_description,notification_type_manifest,notification_type_conf_content FROM t_notification_type WHERE notification_type_id=%i",&notification_type_id);
	
	if(!db->FetchRow())
		throw Exception("NotificationType","Unknown notification type");
	
	id = db->GetFieldInt(0);
	scope = db->GetField(1);
	name = db->GetField(2);
	description = db->GetField(3);
	manifest = db->GetField(4);
	configuration = db->GetField(5);
}

void NotificationType::PutFile(const string &filename,const string &data,bool base64_encoded)
{
	if(base64_encoded)
		FileManager::PutFile(ConfigurationEvQueue::GetInstance()->Get("notifications.tasks.directory"),filename,data,FileManager::FILETYPE_CONF,FileManager::DATATYPE_BASE64);
	else
		FileManager::PutFile(ConfigurationEvQueue::GetInstance()->Get("notifications.tasks.directory"),filename,data,FileManager::FILETYPE_CONF,FileManager::DATATYPE_BINARY);
	
	FileManager::Chmod(ConfigurationEvQueue::GetInstance()->Get("notifications.tasks.directory"),filename,S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
}

void NotificationType::GetFile(const string &filename,string &data)
{
	FileManager::GetFile(ConfigurationEvQueue::GetInstance()->Get("notifications.tasks.directory"),filename,data);
}

void NotificationType::GetFileHash(const string &filename,string &hash)
{
	FileManager::GetFileHash(ConfigurationEvQueue::GetInstance()->Get("notifications.tasks.directory"),filename,hash);
}

void NotificationType::RemoveFile(const string &filename)
{
	FileManager::RemoveFile(ConfigurationEvQueue::GetInstance()->Get("notifications.tasks.directory"),filename);
}

void NotificationType::Get(unsigned int id, QueryResponse *response)
{
	NotificationType type = NotificationTypes::GetInstance()->Get(id);
	
	DOMElement node = (DOMElement)response->AppendXML("<notification_type />");
	node.setAttribute("scope",type.GetScope());
	node.setAttribute("name",type.GetName());
	node.setAttribute("description",type.GetDescription());
	response->AppendXML(type.GetManifest());
}

unsigned int NotificationType::Register(const string &zip_data)
{
	// Open Zip archive
	Zip zip(zip_data);
	string manifest = zip.GetFile("manifest.xml");
	
	// Load manifest file
	unique_ptr<DOMDocument> xmldoc(DOMDocument::Parse(manifest));
	
	// Read scope
	string scope = "WORKFLOW";
	unique_ptr<DOMXPathResult> res_scope(xmldoc->evaluate("/plugin/scope",xmldoc->getDocumentElement(),DOMXPathResult::FIRST_RESULT_TYPE));
	if(res_scope->isNode())
		scope = res_scope->getNodeValue().getTextContent();
	
	// Read name
	unique_ptr<DOMXPathResult> res_name(xmldoc->evaluate("/plugin/name",xmldoc->getDocumentElement(),DOMXPathResult::FIRST_RESULT_TYPE));
	if(!res_name->isNode())
		throw Exception("NotificationType","Invalid manifest, could not find 'name' tag","INVALID_PARAMETER");

	string name = res_name->getNodeValue().getTextContent();
	
	if(name.length()==0)
		throw Exception("NotificationType","Name cannot be empty","INVALID_PARAMETER");
	
	// Read description
	unique_ptr<DOMXPathResult> res_desc(xmldoc->evaluate("/plugin/description",xmldoc->getDocumentElement(),DOMXPathResult::FIRST_RESULT_TYPE));
	if(!res_desc->isNode())
		throw Exception("NotificationType","Invalid manifest, could not find 'description' tag","INVALID_PARAMETER");

	string description = res_desc->getNodeValue().getTextContent();
	
	// Read file name
	unique_ptr<DOMXPathResult> res_bin(xmldoc->evaluate("/plugin/binary",xmldoc->getDocumentElement(),DOMXPathResult::FIRST_RESULT_TYPE));
	if(!res_bin->isNode())
		throw Exception("NotificationType","Invalid manifest, could not find 'binary' tag","INVALID_PARAMETER");

	string binary = res_bin->getNodeValue().getTextContent();
	
	// Read binary from Zip archive
	string binary_data = zip.GetFile(binary);
	
	DB db;
	db.QueryPrintf(
		"INSERT INTO t_notification_type(notification_type_scope,notification_type_name,notification_type_description,notification_type_manifest,notification_type_binary_content) \
		VALUES(%s,%s,%s,%s,%s) \
		ON DUPLICATE KEY UPDATE \
			notification_type_description=VALUES(notification_type_description), \
			notification_type_manifest=VALUES(notification_type_manifest), \
			notification_type_binary_content=VALUES(notification_type_binary_content \
		)",
		&scope,
		&name,
		&description,
		&manifest,
		&binary_data
		);
	
	return db.InsertID();
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
		RemoveFile(to_string(notification_type.id));
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

bool NotificationType::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	const string action = query->GetRootAttribute("action");
	
	if(action=="get")
	{
		unsigned int id = query->GetRootAttributeInt("id");
		
		Get(id, response);
		
		return true;
	}
	else if(action=="register")
	{
		string zip_data_base64 = query->GetRootAttribute("zip");
		string zip_data;
		if(!base64_decode_string(zip_data,zip_data_base64))
			throw Exception("NotificationType","Invalid base64 zip","INVALID_PARAMETER");
		
		unsigned int id = Register(zip_data);
		
		NotificationTypes::GetInstance()->Reload();
		NotificationTypes::GetInstance()->SyncBinaries();
		Notifications::GetInstance()->Reload();
		Workflows::GetInstance()->Reload();
		
		Events::GetInstance()->Create("NOTIFICATION_TYPE_CREATED");
		
		return true;
	}
	else if(action=="unregister")
	{
		unsigned int id = query->GetRootAttributeInt("id");
		
		Unregister(id);
		
		NotificationTypes::GetInstance()->Reload();
		NotificationTypes::GetInstance()->SyncBinaries();
		Notifications::GetInstance()->Reload();
		Workflows::GetInstance()->Reload();
		
		Events::GetInstance()->Create("NOTIFICATION_TYPE_REMOVED");
		
		return true;
	}
	else if(action=="get_conf")
	{
		unsigned int id = query->GetRootAttributeInt("id");
		
		GetConf(id, response);
		
		return true;
	}
	else if(action=="set_conf")
	{
		unsigned int id = query->GetRootAttributeInt("id");
		string content_base64 = query->GetRootAttribute("content","");
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
