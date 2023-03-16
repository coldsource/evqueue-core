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

#include <Storage/Launcher.h>
#include <Storage/Launchers.h>
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
	qh->RegisterHandler("launcher", Launcher::HandleQuery);
	
	Events::GetInstance()->RegisterEvents({"LAUNCHER_CREATED", "LAUNCHER_MODIFIED", "LAUNCHER_REMOVED"});
	return (APIAutoInit *)0;
});

Launcher::Launcher(DB *db,unsigned int launcher_id)
{
	db->QueryPrintf(" SELECT \
			launcher_id, \
			launcher_name, \
			launcher_group, \
			launcher_description, \
			workflow_id, \
			launcher_user, \
			launcher_host, \
			launcher_parameters \
		FROM t_launcher \
		WHERE launcher_id=%i",{&launcher_id});
	
	if(!db->FetchRow())
		throw Exception("Launcher","Unknown Launcher");
	
	this->launcher_id = db->GetFieldInt(0);
	name = db->GetField(1);
	group = db->GetField(2);
	description = db->GetField(3);
	workflow_id = db->GetFieldInt(4);
	user = db->GetField(5);
	host = db->GetField(6);
	parameters = db->GetField(7);
}

void Launcher::create_edit_check(
	const string &name,
	const string &group,
	const string &description,
	unsigned int workflow_id,
	const string &user,
	const string &host,
	const string &parameters)
{
	if(name=="")
		throw Exception("Launcher", "Name cannot be empty", "INVALID_PARAMETER");
	
	if(name.size()>STORAGE_NAME_MAXLEN)
		throw Exception("Launcher", "Name is too long", "INVALID_PARAMETER");
	
	if(group.size()>STORAGE_NAME_MAXLEN)
		throw Exception("Launcher", "Group is too long", "INVALID_PARAMETER");
	
	if(description.size()>STORAGE_DESC_MAXLEN)
		throw Exception("Launcher", "Description is too long", "INVALID_PARAMETER");
	
	auto wf = Workflows::GetInstance()->Get(workflow_id);
	
	if(user.size()>STORAGE_PATH_MAXLEN)
		throw Exception("Launcher", "User variable path is too long", "INVALID_PARAMETER");
	
	if(host.size()>STORAGE_PATH_MAXLEN)
		throw Exception("Launcher", "User variable path is too long", "INVALID_PARAMETER");
	
	if(parameters.size()>STORAGE_PATH_MAXLEN)
		throw Exception("Launcher", "Parameters json is too long", "INVALID_PARAMETER");
	
	try
	{
		auto j = json::parse(parameters);
	}
	catch(...)
	{
		throw Exception("Storage", "Invalid json in parameters", "INVALID_PARAMETER");
	}
}

unsigned int Launcher::Create(
	const string &name,
	const string &group,
	const string &description,
	unsigned int workflow_id,
	const string &user,
	const string &host,
	const string &parameters)
{
	create_edit_check(name, group, description, workflow_id, user, host, parameters);
	
	DB db;
	
	db.QueryPrintf(" \
		INSERT INTO t_launcher(launcher_name, launcher_group, launcher_description, workflow_id, launcher_user, launcher_host, launcher_parameters) \
		VALUES(%s, %s, %s, %i, %s, %s, %s)",
		{&name, &group, &description, &workflow_id, &user, &host, &parameters}
	);
	
	return db.InsertID();
}

void Launcher::Edit(
	unsigned int id,
	const string &name,
	const string &group,
	const string &description,
	unsigned int workflow_id,
	const string &user,
	const string &host,
	const string &parameters)
{
	create_edit_check(name, group, description, workflow_id, user, host, parameters);
	
	DB db;
	
	db.QueryPrintf(" \
		UPDATE t_launcher SET \
			launcher_name = %s, \
			launcher_group = %s, \
			launcher_description = %s, \
			workflow_id = %i, \
			launcher_user = %s, \
			launcher_host = %s,  \
			launcher_parameters = %s \
		WHERE launcher_id = %i",
		{&name, &group, &description, &workflow_id, &user, &host, &parameters, &id}
	);
}

void Launcher::Get(unsigned int id, QueryResponse *response)
{
	const Launcher launcher = Launchers::GetInstance()->Get(id);
	
	response->SetAttribute("name", launcher.GetName());
	response->SetAttribute("group", launcher.GetGroup());
	response->SetAttribute("description", launcher.GetDescription());
	response->SetAttribute("workflow_id", to_string(launcher.GetWorkflowID()));
	response->SetAttribute("user", launcher.GetUser());
	response->SetAttribute("host", launcher.GetHost());
	response->SetAttribute("parameters", launcher.GetParameters());
}

void Launcher::Delete(unsigned int id)
{
	DB db;
	
	db.QueryPrintf("DELETE FROM t_launcher WHERE launcher_id=%i", {&id});
	User::RevokeModuleRight("storage", "launcher", id);
	
	if(db.AffectedRows()==0)
		throw Exception("User","Launcher not found","UNKNOWN_LAUNCHER");
}

bool Launcher::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	string action = query->GetRootAttribute("action");
	
	if(action=="create" || action=="edit")
	{
		if(!user.IsAdmin())
			User::InsufficientRights();
		
		string name = query->GetRootAttribute("name");
		string group = query->GetRootAttribute("group", "");
		string description = query->GetRootAttribute("description", "");
		string user = query->GetRootAttribute("user");
		string host = query->GetRootAttribute("host");
		string parameters = query->GetRootAttribute("parameters");
		unsigned int workflow_id = query->GetRootAttributeInt("workflow_id");
		
		unsigned int id;
		string event = "";
		
		if(action=="create")
		{
			id = Create(name, group, description, workflow_id, user, host, parameters);
			response->GetDOM()->getDocumentElement().setAttribute("launcher-id",to_string(id));
			
			event = "LAUNCHER_CREATED";
		}
		else
		{
			id = query->GetRootAttributeInt("id");
			
			Edit(id, name, group, description, workflow_id, user, host, parameters);
			
			event = "LAUNCHER_MODIFIED";
		}
		
		Launchers::GetInstance()->Reload();
		
		Events::GetInstance()->Create(event, id);
		
		return true;
	}
	else if(action=="get")
	{
		unsigned int id = query->GetRootAttributeInt("id");
		
		if(!user.HasAccessToModule("storage", "launcher", id))
			User::InsufficientRights();
		
		Get(id, response);
		
		return true;
	}
	else if(action=="delete")
	{
		if(!user.IsAdmin())
			User::InsufficientRights();
		
		unsigned int id = query->GetRootAttributeInt("id");
		
		Delete(id);
		
		Launchers::GetInstance()->Reload();
		
		Events::GetInstance()->Create("LAUNCHER_REMOVED", id);
		
		return true;
	}
	else if(action=="launch")
	{
		unsigned int id = query->GetRootAttributeInt("id");
		
		if(!user.HasAccessToModule("storage", "launcher", id))
			User::InsufficientRights();
		
		auto launcher = Launchers::GetInstance()->Get(id);
		Workflow workflow = Workflows::GetInstance()->Get(launcher.GetWorkflowID());
		
		WorkflowInstanceAPI::Launch(workflow.GetName(), user, query, response);
		
		return true;
	}
	else if(action=="grant")
	{
		if(!user.IsAdmin())
			User::InsufficientRights();
		
		unsigned int user_id = query->GetRootAttributeInt("user_id");
		unsigned int launcher_id = query->GetRootAttributeInt("launcher_id");
		
		// Check objects exist
		Users::GetInstance()->Get(user_id);
		Launchers::GetInstance()->Get(launcher_id);
		
		User::GrantModuleRight(user_id, "storage", "launcher", launcher_id);
		
		Users::GetInstance()->Reload();
		
		Events::GetInstance()->Create("USER_MODIFIED", user_id);
		
		return true;
	}
	else if(action=="revoke")
	{
		if(!user.IsAdmin())
			User::InsufficientRights();
		
		unsigned int user_id = query->GetRootAttributeInt("user_id");
		unsigned int launcher_id = query->GetRootAttributeInt("launcher_id");
		
		// Check objects exist
		Users::GetInstance()->Get(user_id);
		Launchers::GetInstance()->Get(launcher_id);
		
		User::RevokeModuleRight(user_id, "storage", "launcher", launcher_id);
		
		Users::GetInstance()->Reload();
		
		Events::GetInstance()->Create("USER_MODIFIED", user_id);
		
		return true;
	}
	
	return false;
}

}
