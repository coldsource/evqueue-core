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

#include <Logs/ChannelGroups.h>
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

ChannelGroups *ChannelGroups::instance = 0;

using namespace std;

ChannelGroups::ChannelGroups():APIObjectList()
{
	instance = this;
	
	Reload(false);
}

ChannelGroups::~ChannelGroups()
{
}

void ChannelGroups::Reload(bool notify)
{
	Logger::Log(LOG_NOTICE,"Reloading channel groups definitions");
	
	unique_lock<mutex> llock(lock);
	
	clear();
	
	// Update
	DB db("elog");
	DB db2(&db);
	db.Query("SELECT channel_group_id, channel_group_name FROM t_channel_group");
	
	while(db.FetchRow())
		add(db.GetFieldInt(0),db.GetField(1),new ChannelGroup(&db2,db.GetFieldInt(0)));
	
	llock.unlock();
	
	if(notify)
	{
		// Notify cluster
		Cluster::GetInstance()->Notify("<control action='reload' module='channelgroups' notify='no' />\n");
	}
}

bool ChannelGroups::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	ChannelGroups *channelgroups = ChannelGroups::GetInstance();
	
	string action = query->GetRootAttribute("action");
	
	if(action=="list")
	{
		unique_lock<mutex> llock(channelgroups->lock);
		
		for(auto it = channelgroups->objects_name.begin(); it!=channelgroups->objects_name.end(); it++)
		{
			ChannelGroup it_channelgroup = *it->second;
			DOMElement node = (DOMElement)response->AppendXML("<channelgroup />");
			node.setAttribute("id",to_string(it_channelgroup.GetID()));
			node.setAttribute("name",it_channelgroup.GetName());
		}
		
		return true;
	}
	
	return false;
}
