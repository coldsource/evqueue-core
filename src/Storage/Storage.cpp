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

#include <Storage/Storage.h>
#include <User/User.h>
#include <Logger/LoggerAPI.h>
#include <Exception/Exception.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <DB/DB.h>
#include <WS/Events.h>
#include <global.h>

using namespace std;

Storage::Storage()
{
}

void Storage::Get(const string &ns, const string &type, const string label, QueryResponse *response)
{
	DB db;
	
	db.QueryPrintf("SELECT storage_id, storage_label, storage_value FROM t_storage WHERE storage_namespace=%s AND storage_type=%s AND storage_label=%s",&ns, &type, &label);
	while(db.FetchRow())
	{
		DOMElement node = (DOMElement)response->AppendXML("<storage />");
		node.setAttribute("id",db.GetField(0));
		node.setAttribute("label",db.GetField(1));
		node.setTextContent(db.GetField(2));
	}
}

void Storage::GetType(const string &ns, const string &type, QueryResponse *response)
{
	DB db;
	
	db.QueryPrintf("SELECT storage_id, storage_label, storage_value FROM t_storage WHERE storage_namespace=%s AND storage_type=%s",&ns, &type);
	while(db.FetchRow())
	{
		DOMElement node = (DOMElement)response->AppendXML("<storage />");
		node.setAttribute("id",db.GetField(0));
		node.setAttribute("label",db.GetField(1));
		node.setTextContent(db.GetField(2));
	}
}

unsigned int Storage::Set(const string &ns, const string &type, const string label, const string value)
{
	DB db;
	
	db.QueryPrintf("INSERT INTO t_storage(storage_namespace, storage_type, storage_label, storage_value) VALUES(%s, %s, %s, %s) ON DUPLICATE KEY UPDATE storage_value=%s",&ns, &type, &label, &value, &value);
	return db.InsertID();
}

bool Storage::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	const string action = query->GetRootAttribute("action");
	
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	if(action=="get")
	{
		string ns = query->GetRootAttribute("ns");
		string type = query->GetRootAttribute("type");
		string label = query->GetRootAttribute("label");
		
		Get(ns, type, label, response);
		
		return true;
	}
	if(action=="gettype")
	{
		string ns = query->GetRootAttribute("ns");
		string type = query->GetRootAttribute("type");
		
		GetType(ns, type, response);
		
		return true;
	}
	else if(action=="set")
	{
		string ns = query->GetRootAttribute("ns");
		string type = query->GetRootAttribute("type");
		string label = query->GetRootAttribute("label");
		string value = query->GetRootAttribute("value");
		
		unsigned int id = Set(ns, type, label, value);
		response->GetDOM()->getDocumentElement().setAttribute("storage-id",to_string(id));
		
		return true;
	}
	
	return false;
}

