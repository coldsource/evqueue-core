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

#include <User/User.h>
#include <User/Users.h>
#include <Workflow/Workflows.h>
#include <Exception/Exception.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <DB/DB.h>
#include <Crypto/Sha1String.h>
#include <global.h>
#include <WS/Events.h>
#include <Logger/LoggerAPI.h>

using namespace std;

User User::anonymous;

User::User()
{
}

User::User(DB *db,unsigned int user_id)
{
	db->QueryPrintf("SELECT user_id, user_login,user_password,user_profile,user_preferences FROM t_user WHERE user_id=%i",&user_id);
	
	if(!db->FetchRow())
		throw Exception("User","Unknown User");
	
	this->user_id = db->GetFieldInt(0);
	user_name = db->GetField(1);
	user_password = db->GetField(2);
	user_profile = db->GetField(3);
	user_preferences = db->GetField(4);
	
	// Load rights
	db->QueryPrintf("SELECT workflow_id, user_right_edit, user_right_read, user_right_exec, user_right_kill FROM t_user_right WHERE user_id=%i",&user_id);
	while(db->FetchRow())
	{
		unsigned int workflow_id = db->GetFieldInt(0);
		user_right wf_rights = {db->GetFieldInt(1)!=0, db->GetFieldInt(2)!=0, db->GetFieldInt(3)!=0, db->GetFieldInt(4)!=0};
		rights[workflow_id] = wf_rights;
	}
}

void User::InitAnonymous()
{
	anonymous.user_name = "anonymous";
	anonymous.user_profile = "ADMIN";
}

bool User::HasAccessToWorkflow(unsigned int workflow_id, const string &access_type) const
{
	if(IsAdmin())
		return true;
	
	auto it = rights.find(workflow_id);
	if(it==rights.end())
		return false;
	
	if(access_type=="edit" && it->second.edit)
		return true;
	else if(access_type=="read" && it->second.read)
		return true;
	else if(access_type=="exec" && it->second.exec)
		return true;
	else if(access_type=="kill" && it->second.kill)
		return true;
	
	return false;
}

vector<int> User::GetReadAccessWorkflows() const
{
	vector<int> read_workflows;
	for(auto it=rights.begin();it!=rights.end();++it)
	{
		if(it->second.read)
			read_workflows.push_back(it->first);
	}
	
	return read_workflows;
}

bool User::InsufficientRights()
{
	throw Exception("Access Control", "insufficient rights","INSUFFICIENT_RIGHTS");
}

bool User::CheckUserName(const string &user_name)
{
	int i,len;
	
	len = user_name.length();
	if(len==0 || len>USER_NAME_MAX_LEN)
		return false;
	
	for(i=0;i<len;i++)
		if(!isalnum(user_name[i]) && user_name[i]!='_' && user_name[i]!='-')
			return false;
	
	return true;
}

void User::Get(unsigned int id, QueryResponse *response)
{
	User user = Users::GetInstance()->Get(id);
	
	DOMElement node = (DOMElement)response->AppendXML("<user />");
	node.setAttribute("id",to_string(user.GetID()));
	node.setAttribute("name",user.GetName());
	node.setAttribute("profile",user.GetProfile());
	node.setAttribute("preferences",user.GetPreferences());
	
	for(auto it=user.rights.begin();it!=user.rights.end();it++)
	{
		DOMElement right_node = response->GetDOM()->createElement("right");
		right_node.setAttribute("workflow-id",to_string(it->first));
		right_node.setAttribute("edit",it->second.edit?"yes":"no");
		right_node.setAttribute("read",it->second.read?"yes":"no");
		right_node.setAttribute("exec",it->second.exec?"yes":"no");
		right_node.setAttribute("kill",it->second.kill?"yes":"no");
		
		node.appendChild(right_node);
	}
}

unsigned int User::Create(const string &name, const string &password, const string &profile)
{
	create_edit_check(name,password,profile);
	
	if(password=="")
		throw Exception("User","Empty password","INVALID_PARAMETER");
	
	string password_sha1 = Sha1String(password).GetHex();
	
	DB db;
	db.QueryPrintf("INSERT INTO t_user(user_login,user_password,user_profile,user_preferences) VALUES(%s,%s,%s,'')",
		&name,
		&password_sha1,
		&profile
	);
	
	return db.InsertID();
}

void User::Edit(unsigned int id, const std::string &name, const string &password, const string &profile)
{
	create_edit_check(name,password,profile);
	
	if(!Users::GetInstance()->Exists(id))
		throw Exception("User","User not found","UNKNOWN_USER");
	
	DB db;
	db.QueryPrintf("UPDATE t_user SET user_login=%s, user_profile=%s WHERE user_id=%i",
		&name,
		&profile,
		&id
	);
	
	if(password!="")
	{
		string password_sha1 = Sha1String(password).GetHex();
		
		db.QueryPrintf("UPDATE t_user SET user_password=%s WHERE user_id=%i",
			&password_sha1,
			&id
		);
	}
}

void User::ChangePassword(unsigned int id,const string &password)
{
	if(!Users::GetInstance()->Exists(id))
		throw Exception("User","User not found","UNKNOWN_USER");
	
	if(password=="")
		throw Exception("User","Invalid password","INVALID_PARAMETER");
	
	string password_sha1 = Sha1String(password).GetHex();
	
	DB db;
	db.QueryPrintf("UPDATE t_user SET user_password=%s WHERE user_id=%i",
		&password_sha1,
		&id
	);
}

void User::UpdatePreferences(unsigned int id, const string &preferences)
{
	if(!Users::GetInstance()->Exists(id))
		throw Exception("User","User not found","UNKNOWN_USER");
	
	DB db;
	db.QueryPrintf("UPDATE t_user SET user_preferences=%s WHERE user_id=%i",
		&preferences,
		&id
	);
}

void User::Delete(unsigned int id)
{
	DB db;
	
	db.StartTransaction();
	
	db.QueryPrintf("DELETE FROM t_user WHERE user_id=%i",&id);
	
	if(db.AffectedRows()==0)
		throw Exception("User","User not found","UNKNOWN_USER");
	
	db.QueryPrintf("DELETE FROM t_user_right WHERE user_id=%i",&id);
	
	db.CommitTransaction();
}

void User::ClearRights(unsigned int id)
{
	if(!Users::GetInstance()->Exists(id))
		throw Exception("User","User not found","UNKNOWN_USER");
	
	DB db;
	db.QueryPrintf("DELETE FROM t_user_right WHERE user_id=%i",&id);
}

void User::ListRights(unsigned int id, QueryResponse *response)
{
	User user = Users::GetInstance()->Get(id);
	
	for(auto it=user.rights.begin();it!=user.rights.end();++it)
	{
		DOMElement node = (DOMElement)response->AppendXML("<right />");
		node.setAttribute("workflow-id",to_string(it->first));
		node.setAttribute("read",it->second.read?"yes":"no");
		node.setAttribute("edit",it->second.edit?"yes":"no");
		node.setAttribute("exec",it->second.exec?"yes":"no");
		node.setAttribute("kill",it->second.kill?"yes":"no");
	}
}

void User::GrantRight(unsigned int id, unsigned int workflow_id, bool edit, bool read, bool exec, bool kill)
{
	if(!Users::GetInstance()->Exists(id))
		throw Exception("User","User not found","UNKNOWN_USER");
	
	if(!Workflows::GetInstance()->Exists(workflow_id))
		throw Exception("User","Workflow not found","UNKNOWN_WORKFLOW");
	
	DB db;
	int iedit = edit, iread = read, iexec = exec, ikill = kill;
	db.QueryPrintf(
		"REPLACE INTO t_user_right(user_id, workflow_id, user_right_edit, user_right_read, user_right_exec, user_right_kill) VALUES(%i,%i,%i,%i,%i,%i)",
		&id, &workflow_id, &iedit, &iread, &iexec, &ikill
	);
}

void User::RevokeRight(unsigned int id, unsigned int workflow_id)
{
	if(!Users::GetInstance()->Exists(id))
		throw Exception("User","User not found","UNKNOWN_USER");
	
	DB db;
	db.QueryPrintf("DELETE FROM t_user_right WHERE user_id=%i AND workflow_id=%i",&id, &workflow_id);
	
	if(db.AffectedRows()==0)
		throw Exception("User","No right found on that workflow","ALREADY_NO_RIGHTS");
}

void User::create_edit_check(const std::string &name, const std::string &password, const std::string &profile)
{
	if(!CheckUserName(name))
		throw Exception("User","Invalid user name","INVALID_PARAMETER");
	
	if(profile!="ADMIN" && profile!="USER")
		throw Exception("User","Invalid profile, should be 'ADMIN' or 'USER'","INVALID_PARAMETER");
}

unsigned int User::get_id_from_query(XMLQuery *query)
{
	unsigned int id = query->GetRootAttributeInt("id", 0);
	if(id!=0)
		return id;
	
	string name = query->GetRootAttribute("name","");
	if(name=="")
		throw Exception("User","Missing 'id' or 'name' attribute","MISSING_PARAMETER");
	
	return Users::GetInstance()->Get(name).GetID();
}

bool User::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	const string action = query->GetRootAttribute("action");
	
	if(action=="get")
	{
		unsigned int id = get_id_from_query(query);
		
		if(!user.IsAdmin() && user.GetID()!=id)
			User::InsufficientRights();
		
		Get(id,response);
		
		return true;
	}
	else if(action=="create" || action=="edit")
	{
		string name = query->GetRootAttribute("name");
		string password = query->GetRootAttribute("password", "");
		string profile = query->GetRootAttribute("profile");
		
		if(action=="create")
		{
			if(!user.IsAdmin())
				User::InsufficientRights();
			
			unsigned int id = Create(name, password, profile);
			response->GetDOM()->getDocumentElement().setAttribute("user-id",to_string(id));
			
			LoggerAPI::LogAction(user,id,"User",query->GetQueryGroup(),action);
			
			Events::GetInstance()->Create(Events::en_types::USER_CREATED, id);
		}
		else
		{
			if(!user.IsAdmin())
				User::InsufficientRights();
			
			unsigned int id = get_id_from_query(query);
			
			Edit(id, name, password, profile);
			
			LoggerAPI::LogAction(user,id,"User",query->GetQueryGroup(),action);
			
			Events::GetInstance()->Create(Events::en_types::USER_MODIFIED, id);
		}
		
		Users::GetInstance()->Reload();
		
		return true;
	}
	else if(action=="change_password")
	{
		unsigned int id = get_id_from_query(query);
		
		string password = query->GetRootAttribute("password");
		
		if(!user.IsAdmin() && user.GetID()!=id)
			User::InsufficientRights();
		
		ChangePassword(id,password);
		
		Users::GetInstance()->Reload();
		
		LoggerAPI::LogAction(user,id,"User",query->GetQueryGroup(),action);
		
		Events::GetInstance()->Create(Events::en_types::USER_MODIFIED, id);
		
		return true;
	}
	else if(action=="update_preferences")
	{
		unsigned int id = get_id_from_query(query);
		string preferences = query->GetRootAttribute("preferences");
		
		if(!user.IsAdmin() && user.GetID()!=id)
			User::InsufficientRights();
		
		UpdatePreferences(id,preferences);
		
		Users::GetInstance()->Reload();
		
		LoggerAPI::LogAction(user,id,"User",query->GetQueryGroup(),action);
		
		Events::GetInstance()->Create(Events::en_types::USER_MODIFIED, id);
		
		return true;
	}
	else if(action=="delete")
	{
		if(!user.IsAdmin())
			User::InsufficientRights();
		
		unsigned int id = get_id_from_query(query);
		
		Delete(id);
		
		Users::GetInstance()->Reload();
		
		LoggerAPI::LogAction(user,id,"User",query->GetQueryGroup(),action);
		
		Events::GetInstance()->Create(Events::en_types::USER_REMOVED, id);
		
		return true;
	}
	else if(action=="clear_rights")
	{
		if(!user.IsAdmin())
			User::InsufficientRights();
		
		unsigned int id = get_id_from_query(query);
		
		ClearRights(id);
		
		Users::GetInstance()->Reload();
		
		LoggerAPI::LogAction(user,id,"User",query->GetQueryGroup(),action);
		
		Events::GetInstance()->Create(Events::en_types::USER_MODIFIED, id);
		
		return true;
	}
	else if(action=="list_rights")
	{
		if(!user.IsAdmin())
			User::InsufficientRights();
		
		unsigned int id = get_id_from_query(query);
		
		ListRights(id, response);
		
		LoggerAPI::LogAction(user,id,"User",query->GetQueryGroup(),action);
		
		Events::GetInstance()->Create(Events::en_types::USER_MODIFIED, id);
		
		return true;
	}
	else if(action=="grant")
	{
		if(!user.IsAdmin())
			User::InsufficientRights();
		
		unsigned int id = get_id_from_query(query);
		unsigned int workflow_id = query->GetRootAttributeInt("workflow_id");
		bool edit = query->GetRootAttributeBool("edit");
		bool read = query->GetRootAttributeBool("read");
		bool exec = query->GetRootAttributeBool("exec");
		bool kill = query->GetRootAttributeBool("kill");
		
		GrantRight(id, workflow_id, edit, read, exec, kill);
		
		Users::GetInstance()->Reload();
		
		LoggerAPI::LogAction(user,id,"User",query->GetQueryGroup(),action);
		
		Events::GetInstance()->Create(Events::en_types::USER_MODIFIED, id);
		
		return true;
	}
	else if(action=="revoke")
	{
		if(!user.IsAdmin())
			User::InsufficientRights();
		
		unsigned int id = get_id_from_query(query);
		unsigned int workflow_id = query->GetRootAttributeInt("workflow_id");
		
		RevokeRight(id, workflow_id);
		
		Users::GetInstance()->Reload();
		
		LoggerAPI::LogAction(user,id,"User",query->GetQueryGroup(),action);
		
		Events::GetInstance()->Create(Events::en_types::USER_MODIFIED, id);
		
		return true;
	}
	
	return false;
}
