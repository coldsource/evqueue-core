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

#include <Tag/Tag.h>
#include <Tag/Tags.h>
#include <User/User.h>
#include <Workflow/Workflows.h>
#include <Logger/LoggerAPI.h>
#include <Exception/Exception.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <DB/DB.h>
#include <WS/Events.h>
#include <API/QueryHandlers.h>
#include <global.h>

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	qh->RegisterHandler("tag", Tag::HandleQuery);
	Events::GetInstance()->RegisterEvents({"TAG_CREATED","TAG_MODIFIED","TAG_REMOVED"});
	return (APIAutoInit *)0;
});

using namespace std;

Tag::Tag()
{
}

Tag::Tag(DB *db,unsigned int id)
{
	db->QueryPrintf("SELECT tag_label FROM t_tag WHERE tag_id=%i",{&id});
	
	if(!db->FetchRow())
		throw Exception("Tag","Unknown Tag","UNKNOWN_TAG");
	
	this->id = id;
	label = db->GetField(0);
}

void Tag::Get(unsigned int id, QueryResponse *response)
{
	Tag tag = Tags::GetInstance()->Get(id);
	
	DOMElement node = (DOMElement)response->AppendXML("<tag />");
	node.setAttribute("label",tag.GetLabel());
}

unsigned int Tag::Create(const string &label)
{
	if(label=="")
		throw Exception("Tag", "Invalid tag label","INVALID_PARAMETER");
	
	DB db;
	
	db.QueryPrintf("SELECT tag_id FROM t_tag WHERE tag_label=%s",{&label});
	if(db.FetchRow())
		return db.GetFieldInt(0);
	
	db.QueryPrintf("INSERT INTO t_tag(tag_label) VALUES(%s)",{&label});
	
	Tags::GetInstance()->Reload();
		
	Events::GetInstance()->Create("TAG_CREATED");
	
	return db.InsertID();
}

void Tag::Edit(unsigned int id, const string &label)
{
	if(label=="")
		throw Exception("Tag", "Invalid tag label","INVALID_PARAMETER");
	
	DB db;
	db.QueryPrintf("UPDATE t_tag SET tag_label=%s WHERE tag_id=%i",{&label,&id});
	if(db.AffectedRows()==0)
		throw Exception("Tag","Tag not found");
}

void Tag::Delete(unsigned int id)
{
	DB db;
	
	db.StartTransaction();
	
	db.QueryPrintf("DELETE FROM t_tag WHERE tag_id=%i",{&id});
	
	if(db.AffectedRows()==0)
		throw Exception("Tag","Tag not found","UNKNOWN_TAG");
	
	db.QueryPrintf("DELETE FROM t_workflow_instance_tag WHERE tag_id=%i",{&id});
	
	db.CommitTransaction();
}

bool Tag::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	const string action = query->GetRootAttribute("action");
	
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	if(action=="get")
	{
		unsigned int id = query->GetRootAttributeInt("id");
		
		Get(id,response);
		
		return true;
	}
	else if(action=="create")
	{
		string label = query->GetRootAttribute("label");
		
		unsigned int id = Create(label);
		
		LoggerAPI::LogAction(user,id,"Tag",query->GetQueryGroup(),action);
		
		response->GetDOM()->getDocumentElement().setAttribute("tag-id",to_string(id));
		
		return true;
	}
	else if(action=="edit")
	{
		unsigned int id = query->GetRootAttributeInt("id");
		string label = query->GetRootAttribute("label");
		
		Edit(id,label);
		
		LoggerAPI::LogAction(user,id,"Tag",query->GetQueryGroup(),action);
		
		Tags::GetInstance()->Reload();
		
		Events::GetInstance()->Create("TAG_MODIFIED");
		
		return true;
	}
	else if(action=="delete")
	{
		unsigned int id = query->GetRootAttributeInt("id");
		
		Delete(id);
		
		LoggerAPI::LogAction(user,id,"Tag",query->GetQueryGroup(),action);
		
		Tags::GetInstance()->Reload();
		
		Events::GetInstance()->Create("TAG_REMOVED");
		
		return true;
	}
	
	return false;
}
