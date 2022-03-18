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

#include <Logs/Channels.h>
#include <Logs/LogStorage.h>
#include <User/User.h>
#include <Exception/Exception.h>
#include <DB/DB.h>
#include <Logger/Logger.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <Cluster/Cluster.h>

#include <regex>
#include <map>

Channels *Channels::instance = 0;

using namespace std;

Channels::Channels():APIObjectList()
{
	instance = this;
	
	Reload(false);
}

Channels::~Channels()
{
}

void Channels::Reload(bool notify)
{
	Logger::Log(LOG_NOTICE,"Reloading channels definitions");
	
	unique_lock<mutex> llock(lock);
	
	clear();
	
	// Update
	DB db;
	DB db2(&db);
	db.Query("SELECT elog_channel_id, elog_channel_name FROM t_elog_channel");
	
	while(db.FetchRow())
		add(db.GetFieldInt(0),db.GetField(1),new Channel(&db2,db.GetFieldInt(0)));
	
	llock.unlock();
	
	if(notify)
	{
		// Notify cluster
		Cluster::GetInstance()->Notify("<control action='reload' module='channels' notify='no' />\n");
	}
}

void Channels::Log(const std::string &str)
{
	regex r("([a-zA-Z0-9_-]+)[ ]+");
	
	smatch matches;
	
	try
	{
		if(!regex_search(str, matches, r))
			throw Exception("Channels", "unable to get log message channel");
		
		if(matches.size()!=2)
			throw Exception("Channels", "unable to get log message channel");
		
		string channel_name = matches[1];
		string log_str = str.substr(matches[0].length());
		
		Channel channel = Get(channel_name);
		
		map<string, string> std, custom;
		channel.ParseLog(log_str, std, custom);
		
		LogStorage::GetInstance()->StoreLog(channel.GetID(), std, custom);
	}
	catch(Exception &e)
	{
		Logger::Log(LOG_ERR, "Error parsing extern log in "+e.context+" : "+e.error);
	}
}

bool Channels::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	Channels *channels = Channels::GetInstance();
	
	string action = query->GetRootAttribute("action");
	
	if(action=="list")
	{
		unique_lock<mutex> llock(channels->lock);
		
		for(auto it = channels->objects_name.begin(); it!=channels->objects_name.end(); it++)
		{
			Channel it_channel = *it->second;
			DOMElement node = (DOMElement)response->AppendXML("<channel />");
			node.setAttribute("id",to_string(it_channel.GetID()));
			node.setAttribute("name",it_channel.GetName());
		}
		
		return true;
	}
	
	return false;
}
