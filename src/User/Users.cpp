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

#include <User/Users.h>
#include <User/User.h>
#include <Exception/Exception.h>
#include <DB/DB.h>
#include <Logger/Logger.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <Cluster/Cluster.h>

Users *Users::instance = 0;

using namespace std;

Users::Users():APIObjectList()
{
	instance = this;
	
	Reload(false);
}

Users::~Users()
{
}

void Users::Reload(bool notify)
{
	Logger::Log(LOG_NOTICE,"Reloading users definitions");
	
	unique_lock<mutex> llock(lock);
	
	clear();
	
	// Update
	DB db;
	DB db2(&db);
	db.Query("SELECT user_login FROM t_user");
	
	while(db.FetchRow())
		add(0,db.GetField(0),new User(&db2,db.GetField(0)));
	
	llock.unlock();
	
	if(notify)
	{
		// Notify cluster
		Cluster::GetInstance()->Notify("<control action='reload' module='users' notify='no' />\n");
	}
}

bool Users::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	Users *users = Users::GetInstance();
	
	string action = query->GetRootAttribute("action");
	
	if(action=="list")
	{
		unique_lock<mutex> llock(users->lock);
		
		for(auto it = users->objects_name.begin(); it!=users->objects_name.end(); it++)
		{
			User it_user = *it->second;
			DOMElement node = (DOMElement)response->AppendXML("<user />");
			node.setAttribute("name",it_user.GetName());
			node.setAttribute("profile",it_user.GetProfile());
		}
		
		return true;
	}
	
	return false;
}
