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

#include <Storage/Display.h>
#include <Storage/Displays.h>
#include <API/QueryHandlers.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <User/User.h>
#include <User/Users.h>
#include <DB/DB.h>
#include <Exception/Exception.h>
#include <WS/Events.h>
#include <Workflow/Workflows.h>
#include <Workflow/Workflow.h>
#include <WorkflowInstance/WorkflowInstanceAPI.h>
#include <Storage/Storage.h>

using namespace std;
using nlohmann::json;

namespace Storage
{

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	qh->RegisterHandler("display", Display::HandleQuery);
	
	Events::GetInstance()->RegisterEvents({"DISPLAY_CREATED", "DISPLAY_MODIFIED", "DISPLAY_REMOVED"});
	return (APIAutoInit *)0;
});

Display::Display(DB *db,unsigned int display_id)
{
	db->QueryPrintf(" SELECT \
			display_id, \
			display_name, \
			display_group, \
			storage_path, \
			display_order, \
			display_item_title, \
			display_item_content \
		FROM t_display \
		WHERE display_id=%i",{&display_id});
	
	if(!db->FetchRow())
		throw Exception("Display","Unknown Display");
	
	this->display_id = db->GetFieldInt(0);
	name = db->GetField(1);
	group = db->GetField(2);
	path = db->GetField(3);
	order = db->GetField(4);
	item_title = db->GetField(5);
	item_content = db->GetField(6);
}

void Display::create_edit_check(
	const string &name,
	const string &group,
	const string &path,
	const string &order,
	const string &item_title,
	const string &item_content)
{
	if(name=="")
		throw Exception("Display", "Name cannot be empty", "INVALID_PARAMETER");
	
	if(name.size()>STORAGE_NAME_MAXLEN)
		throw Exception("Display", "Name is too long", "INVALID_PARAMETER");
	
	if(group.size()>STORAGE_NAME_MAXLEN)
		throw Exception("Display", "Name is group long", "INVALID_PARAMETER");
	
	if(order!="ASC" && order!="DESC")
		throw Exception("Display", "Order must be ASC or DESC", "INVALID_PARAMETER");
	
	if(path=="")
		throw Exception("Display", "Variable path can't be empty", "INVALID_PARAMETER");
	
	if(path.size()>STORAGE_PATH_MAXLEN)
		throw Exception("Display", "Variable path is too long", "INVALID_PARAMETER");
	
	if(item_title=="")
		throw Exception("Display", "Item title can't be empty", "INVALID_PARAMETER");
	
	if(item_content=="")
		throw Exception("Display", "Item content can't be empty", "INVALID_PARAMETER");
}

unsigned int Display::Create(
	const string &name,
	const string &group,
	const string &path,
	const string &order,
	const string &item_title,
	const string &item_description)
{
	create_edit_check(name, group, path, order, item_title, item_description);
	
	if(!Displays::GetInstance()->Exists(name))
		throw Exception("Display","Display « " + name + " » already exists","INVALID_PARAMETER");
	
	DB db;
	
	db.QueryPrintf(" \
		INSERT INTO t_display(display_name, display_group, storage_path, display_order, display_item_title, display_item_content) \
		VALUES(%s, %s, %s, %s, %s, %s)",
		{&name, &group, &path, &order, &item_title, &item_description}
	);
	
	return db.InsertID();
}

void Display::Edit(
	unsigned int id,
	const string &name,
	const string &group,
	const string &path,
	const string &order,
	const string &item_title,
	const string &item_description)
{
	create_edit_check(name, group, path, order, item_title, item_description);
	
	DB db;
	
	db.QueryPrintf(" \
		UPDATE t_display SET \
			display_name = %s, \
			display_group = %s, \
			storage_path = %s, \
			display_order = %s, \
			display_item_title = %s, \
			display_item_content = %s \
		WHERE display_id = %i",
		{&name, &group, &path, &order, &item_title, &item_description, &id}
	);
}

void Display::Get(unsigned int id, QueryResponse *response)
{
	const Display display = Displays::GetInstance()->Get(id);
	
	response->SetAttribute("name", display.GetName());
	response->SetAttribute("group", display.GetGroup());
	response->SetAttribute("path", display.GetPath());
	response->SetAttribute("order", display.GetOrder());
	response->SetAttribute("item_title", display.GetItemTitle());
	response->SetAttribute("item_content", display.GetItemContent());
}

void Display::Delete(unsigned int id)
{
	DB db;
	
	db.QueryPrintf("DELETE FROM t_display WHERE display_id=%i", {&id});
	
	if(db.AffectedRows()==0)
		throw Exception("User","Display not found","UNKNOWN_DISPLAY");
}

bool Display::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	string action = query->GetRootAttribute("action");
	
	if(action=="create" || action=="edit")
	{
		if(!user.IsAdmin())
			User::InsufficientRights();
		
		string name = query->GetRootAttribute("name");
		string group = query->GetRootAttribute("group", "");
		string path = query->GetRootAttribute("path");
		string order = query->GetRootAttribute("order");
		string item_title = query->GetRootAttribute("item_title");
		string item_content = query->GetRootAttribute("item_content");
		
		unsigned int id;
		string event = "";
		
		if(action=="create")
		{
			id = Create(name, group, path, order, item_title, item_content);
			response->GetDOM()->getDocumentElement().setAttribute("display-id",to_string(id));
			
			event = "DISPLAY_CREATED";
		}
		else
		{
			id = query->GetRootAttributeInt("id");
			
			Edit(id, name, group, path, order, item_title, item_content);
			
			event = "DISPLAY_MODIFIED";
		}
		
		Displays::GetInstance()->Reload();
		
		Events::GetInstance()->Create(event, id);
		
		return true;
	}
	else if(action=="get")
	{
		unsigned int id = query->GetRootAttributeInt("id");
		
		Get(id, response);
		
		return true;
	}
	else if(action=="delete")
	{
		if(!user.IsAdmin())
			User::InsufficientRights();
		
		unsigned int id = query->GetRootAttributeInt("id");
		
		Delete(id);
		
		Displays::GetInstance()->Reload();
		
		Events::GetInstance()->Create("DISPLAY_REMOVED", id);
		
		return true;
	}
	
	return false;
}

}
