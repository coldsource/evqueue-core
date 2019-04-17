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

#include <Tag.h>
#include <Tags.h>
#include <User.h>
#include <Workflows.h>
#include <Exception.h>
#include <SocketQuerySAX2Handler.h>
#include <QueryResponse.h>
#include <DB.h>
#include <global.h>

using namespace std;

Tag::Tag()
{
}

Tag::Tag(DB *db,unsigned int id)
{
	db->QueryPrintf("SELECT tag_label FROM t_tag WHERE tag_id=%i",&id);
	
	if(!db->FetchRow())
		throw Exception("Tag","Unknown Tag");
	
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
		throw Exception("Tag", "Invalid tag label");
	
	DB db;
	
	db.QueryPrintf("SELECT tag_id FROM t_tag WHERE tag_label=%s",&label);
	if(db.FetchRow())
		return db.GetFieldInt(0);
	
	db.QueryPrintf("INSERT INTO t_tag(tag_label) VALUES(%s)",&label);
	return db.InsertID();
}

void Tag::Edit(unsigned int id, const string &label)
{
	if(label=="")
		throw Exception("Tag", "Invalid tag label");
	
	DB db;
	db.QueryPrintf("UPDATE t_tag SET tag_label=%s WHERE tag_id=%i",&label,&id);
	if(db.AffectedRows()==0)
		throw Exception("Tag","Tag not found");
}

void Tag::Delete(unsigned int id)
{
	DB db;
	
	db.StartTransaction();
	
	db.QueryPrintf("DELETE FROM t_tag WHERE tag_id=%i",&id);
	
	if(db.AffectedRows()==0)
		throw Exception("Tag","Tag not found");
	
	db.QueryPrintf("DELETE FROM t_workflow_instance_tag WHERE tag_id=%i",&id);
	
	db.CommitTransaction();
}

bool Tag::HandleQuery(const User &user, SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	const string action = saxh->GetRootAttribute("action");
	
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	if(action=="get")
	{
		unsigned int id = saxh->GetRootAttributeInt("id");
		
		Get(id,response);
		
		return true;
	}
	else if(action=="create")
	{
		string label = saxh->GetRootAttribute("label");
		
		unsigned int id = Create(label);
		response->GetDOM()->getDocumentElement().setAttribute("tag-id",to_string(id));
		
		Tags::GetInstance()->Reload();
		
		return true;
	}
	else if(action=="edit")
	{
		unsigned int id = saxh->GetRootAttributeInt("id");
		string label = saxh->GetRootAttribute("label");
		
		Edit(id,label);
		
		Tags::GetInstance()->Reload();
		
		return true;
	}
	else if(action=="delete")
	{
		unsigned int id = saxh->GetRootAttributeInt("id");
		
		Delete(id);
		
		Tags::GetInstance()->Reload();
		
		return true;
	}
	
	return false;
}
