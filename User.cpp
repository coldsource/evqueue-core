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

#include <User.h>
#include <Users.h>
#include <Exception.h>
#include <SocketQuerySAX2Handler.h>
#include <QueryResponse.h>
#include <DB.h>
#include <global.h>

#include <xqilla/xqilla-dom3.hpp>

using namespace std;
using namespace xercesc;

User::User()
{
}

User::User(DB *db,const string &user_name)
{
	db->QueryPrintf("SELECT user_login,user_password,user_profile FROM t_user WHERE user_login=%s",user_name.c_str());
	
	if(!db->FetchRow())
		throw Exception("User","Unknown User");
	
	this->user_name = db->GetField(0);
	user_password = db->GetField(1);
	user_profile = db->GetField(2);
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
	
	DOMElement *node = (DOMElement *)response->AppendXML("<user />");
	node->setAttribute(X("name"),X(user.GetName().c_str()));
	node->setAttribute(X("profile"),X(user.GetProfile().c_str()));
}

unsigned int User::Create(const string &name, const string &password, const string &profile)
{
	create_edit_check(name,password,profile);
	
	DB db;
	db.QueryPrintf("INSERT INTO t_user(user_login,user_password,user_profile) VALUES(%s,%s,%s)",
		name.c_str(),
		password.c_str(),
		profile.c_str()
	);
	
	return db.InsertID();
}

void User::Edit(const string &name,const string &password, const string &profile)
{
	create_edit_check(name,password,profile);
	
	if(!Users::GetInstance()->Exists(name))
		throw Exception("User","User not found");
	
	DB db;
	db.QueryPrintf("UPDATE t_user SET user_password=%s,user_profile=%s WHERE user_login=%s",
		password.c_str(),
		profile.c_str(),
		name.c_str()
	);
}

void User::Delete(const string &name)
{
	DB db;
	db.QueryPrintf("DELETE FROM t_user WHERE user_login=%s",name.c_str());
	
	if(db.AffectedRows()==0)
		throw Exception("User","User not found");
}

void User::create_edit_check(const std::string &name, const std::string &password, const std::string &profile)
{
	if(!CheckUserName(name))
		throw Exception("User","Invalid user name");
	
	if(profile!="ADMIN" && profile!="USER")
		throw Exception("User","Invalid profile, should be 'ADMIN' or 'USER'");
}

bool User::HandleQuery(SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	const string action = saxh->GetRootAttribute("action");
	
	if(action=="get")
	{
		string name = saxh->GetRootAttribute("name");
		
		Get(name,response);
		
		return true;
	}
	else if(action=="create" || action=="edit")
	{
		string name = saxh->GetRootAttribute("name");
		string password = saxh->GetRootAttribute("password");
		string profile = saxh->GetRootAttribute("profile");
		
		if(action=="create")
			Create(name, profile, profile);
		else
			Edit(name, profile, profile);
		
		Users::GetInstance()->Reload();
		
		return true;
	}
	else if(action=="delete")
	{
		string name = saxh->GetRootAttribute("name");
		
		Delete(name);
		
		Users::GetInstance()->Reload();
		
		return true;
	}
	
	return false;
}