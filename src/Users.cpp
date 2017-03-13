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

#include <Users.h>
#include <User.h>
#include <Exception.h>
#include <DB.h>
#include <Logger.h>
#include <SocketQuerySAX2Handler.h>
#include <QueryResponse.h>
#include <Cluster.h>

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
	
	pthread_mutex_lock(&lock);
	
	clear();
	
	// Update
	DB db;
	DB db2(&db);
	db.Query("SELECT user_login FROM t_user");
	
	while(db.FetchRow())
		add(0,db.GetField(0),new User(&db2,db.GetField(0)));
	
	pthread_mutex_unlock(&lock);
	
	if(notify)
	{
		// Notify cluster
		Cluster::GetInstance()->ExecuteCommand("<control action='reload' module='users' notify='no' />\n");
	}
}

bool Users::HandleQuery(const User &user, SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	Users *users = Users::GetInstance();
	
	string action = saxh->GetRootAttribute("action");
	
	if(action=="list")
	{
		pthread_mutex_lock(&users->lock);
		
		for(auto it = users->objects_name.begin(); it!=users->objects_name.end(); it++)
		{
			User it_user = *it->second;
			DOMElement node = (DOMElement)response->AppendXML("<user />");
			node.setAttribute("name",it_user.GetName());
			node.setAttribute("profile",it_user.GetProfile());
		}
		
		pthread_mutex_unlock(&users->lock);
		
		return true;
	}
	
	return false;
}