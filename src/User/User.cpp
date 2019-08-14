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
#include <API/SocketQuerySAX2Handler.h>
#include <API/QueryResponse.h>
#include <DB/DB.h>
#include <Crypto/Sha1String.h>
#include <global.h>

using namespace std;

User User::anonymous;

User::User()
{
}

User::User(DB *db,const string &user_name)
{
	db->QueryPrintf("SELECT user_login,user_password,user_profile,user_preferences FROM t_user WHERE user_login=%s",&user_name);
	
	if(!db->FetchRow())
		throw Exception("User","Unknown User");
	
	this->user_name = db->GetField(0);
	user_password = db->GetField(1);
	user_profile = db->GetField(2);
	user_preferences = db->GetField(3);
	
	// Load rights
	db->QueryPrintf("SELECT workflow_id, user_right_edit, user_right_read, user_right_exec, user_right_kill FROM t_user_right WHERE user_login=%s",&user_name);
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

void User::Get(const string &name, QueryResponse *response)
{
	User user = Users::GetInstance()->Get(name);
	
	DOMElement node = (DOMElement)response->AppendXML("<user />");
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
	
	string password_sha1 = Sha1String(password).GetHex();
	
	DB db;
	db.QueryPrintf("INSERT INTO t_user(user_login,user_password,user_profile,user_preferences) VALUES(%s,%s,%s,'')",
		&name,
		&password_sha1,
		&profile
	);
	
	return db.InsertID();
}

void User::Edit(const string &name,const string &password, const string &profile)
{
	create_edit_check(name,password,profile);
	
	if(!Users::GetInstance()->Exists(name))
		throw Exception("User","User not found","UNKNOWN_USER");
	
	string password_sha1 = Sha1String(password).GetHex();
	
	DB db;
	db.QueryPrintf("UPDATE t_user SET user_password=%s,user_profile=%s WHERE user_login=%s",
		&password_sha1,
		&profile,
		&name
	);
}

void User::ChangePassword(const string &name,const string &password)
{
	if(!Users::GetInstance()->Exists(name))
		throw Exception("User","User not found","UNKNOWN_USER");
	
	if(password=="")
		throw Exception("User","Invalid password","INVALID_PARAMETER");
	
	string password_sha1 = Sha1String(password).GetHex();
	
	DB db;
	db.QueryPrintf("UPDATE t_user SET user_password=%s WHERE user_login=%s",
		&password_sha1,
		&name
	);
}

void User::UpdatePreferences(const string &name, const string &preferences)
{
	if(!Users::GetInstance()->Exists(name))
		throw Exception("User","User not found","UNKNOWN_USER");
	
	DB db;
	db.QueryPrintf("UPDATE t_user SET user_preferences=%s WHERE user_login=%s",
		&preferences,
		&name
	);
}

void User::Delete(const string &name)
{
	DB db;
	
	db.StartTransaction();
	
	db.QueryPrintf("DELETE FROM t_user WHERE user_login=%s",&name);
	
	if(db.AffectedRows()==0)
		throw Exception("User","User not found","UNKNOWN_USER");
	
	db.QueryPrintf("DELETE FROM t_user_right WHERE user_login=%s",&name);
	
	db.CommitTransaction();
}

void User::ClearRights(const string &name)
{
	if(!Users::GetInstance()->Exists(name))
		throw Exception("User","User not found","UNKNOWN_USER");
	
	DB db;
	db.QueryPrintf("DELETE FROM t_user_right WHERE user_login=%s",&name);
}

void User::ListRights(const string &name, QueryResponse *response)
{
	User user = Users::GetInstance()->Get(name);
	
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

void User::GrantRight(const string &name, unsigned int workflow_id, bool edit, bool read, bool exec, bool kill)
{
	if(!Users::GetInstance()->Exists(name))
		throw Exception("User","User not found","UNKNOWN_USER");
	
	if(!Workflows::GetInstance()->Exists(workflow_id))
		throw Exception("User","Workflow not found","UNKNOWN_WORKFLOW");
	
	DB db;
	int iedit = edit, iread = read, iexec = exec, ikill = kill;
	db.QueryPrintf(
		"INSERT INTO t_user_right(user_login, workflow_id, user_right_edit, user_right_read, user_right_exec, user_right_kill) VALUES(%s,%i,%i,%i,%i,%i)",
		&name, &workflow_id, &iedit, &iread, &iexec, &ikill
	);
}

void User::RevokeRight(const string &name, unsigned int workflow_id)
{
	if(!Users::GetInstance()->Exists(name))
		throw Exception("User","User not found","UNKNOWN_USER");
	
	DB db;
	db.QueryPrintf("DELETE FROM t_user_right WHERE workflow_id=%i",&workflow_id);
	
	if(db.AffectedRows()==0)
		throw Exception("User","No right found on that workflow","ALREADY_NO_RIGHTS");
}

void User::create_edit_check(const std::string &name, const std::string &password, const std::string &profile)
{
	if(!CheckUserName(name))
		throw Exception("User","Invalid user name","INVALID_PARAMETER");
	
	if(profile!="ADMIN" && profile!="USER")
		throw Exception("User","Invalid profile, should be 'ADMIN' or 'USER'","INVALID_PARAMETER");
	
	if(password=="")
		throw Exception("User","Empty password","INVALID_PARAMETER");
}

bool User::HandleQuery(const User &user, SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	const string action = saxh->GetRootAttribute("action");
	
	if(action=="get")
	{
		string name = saxh->GetRootAttribute("name");
		
		if(!user.IsAdmin() && user.GetName()!=name)
			User::InsufficientRights();
		
		Get(name,response);
		
		return true;
	}
	else if(action=="create" || action=="edit")
	{
		string name = saxh->GetRootAttribute("name");
		string password = saxh->GetRootAttribute("password");
		string profile = saxh->GetRootAttribute("profile");
		
		if(action=="create")
		{
			if(!user.IsAdmin())
				User::InsufficientRights();
			
			unsigned int id = Create(name, password, profile);
			response->GetDOM()->getDocumentElement().setAttribute("user-id",to_string(id));
		}
		else
		{
			if(!user.IsAdmin())
				User::InsufficientRights();
			
			Edit(name, password, profile);
		}
		
		Users::GetInstance()->Reload();
		
		return true;
	}
	else if(action=="change_password")
	{
		string name = saxh->GetRootAttribute("name");
		string password = saxh->GetRootAttribute("password");
		
		if(!user.IsAdmin() && user.GetName()!=name)
			User::InsufficientRights();
		
		ChangePassword(name,password);
		
		Users::GetInstance()->Reload();
		
		return true;
	}
	else if(action=="update_preferences")
	{
		string name = saxh->GetRootAttribute("name");
		string preferences = saxh->GetRootAttribute("preferences");
		
		if(!user.IsAdmin() && user.GetName()!=name)
			User::InsufficientRights();
		
		UpdatePreferences(name,preferences);
		
		Users::GetInstance()->Reload();
		
		return true;
	}
	else if(action=="delete")
	{
		if(!user.IsAdmin())
			User::InsufficientRights();
		
		string name = saxh->GetRootAttribute("name");
		
		Delete(name);
		
		Users::GetInstance()->Reload();
		
		return true;
	}
	else if(action=="clear_rights")
	{
		if(!user.IsAdmin())
			User::InsufficientRights();
		
		string name = saxh->GetRootAttribute("name");
		
		ClearRights(name);
		
		Users::GetInstance()->Reload();
		
		return true;
	}
	else if(action=="list_rights")
	{
		if(!user.IsAdmin())
			User::InsufficientRights();
		
		string name = saxh->GetRootAttribute("name");
		
		ListRights(name, response);
		
		return true;
	}
	else if(action=="grant")
	{
		if(!user.IsAdmin())
			User::InsufficientRights();
		
		string name = saxh->GetRootAttribute("name");
		unsigned int workflow_id = saxh->GetRootAttributeInt("workflow_id");
		bool edit = saxh->GetRootAttributeBool("edit");
		bool read = saxh->GetRootAttributeBool("read");
		bool exec = saxh->GetRootAttributeBool("exec");
		bool kill = saxh->GetRootAttributeBool("kill");
		
		GrantRight(name, workflow_id, edit, read, exec, kill);
		
		Users::GetInstance()->Reload();
		
		return true;
	}
	else if(action=="revoke")
	{
		if(!user.IsAdmin())
			User::InsufficientRights();
		
		string name = saxh->GetRootAttribute("name");
		unsigned int workflow_id = saxh->GetRootAttributeInt("workflow_id");
		
		RevokeRight(name, workflow_id);
		
		Users::GetInstance()->Reload();
		
		return true;
	}
	
	return false;
}
