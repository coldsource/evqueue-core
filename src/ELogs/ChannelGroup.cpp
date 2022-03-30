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

#include <ELogs/ChannelGroup.h>
#include <ELogs/ChannelGroups.h>
#include <ELogs/Channels.h>
#include <Exception/Exception.h>
#include <DB/DB.h>
#include <Logger/Logger.h>
#include <Logger/LoggerAPI.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <User/User.h>
#include <WS/Events.h>
#include <global.h>

#include <time.h>

#include <nlohmann/json.hpp>

using namespace std;
using nlohmann::json;

namespace ELogs
{

ChannelGroup::ChannelGroup(DB *db,unsigned int channel_group_id):fields(Fields::en_type::GROUP, channel_group_id)
{
	db->QueryPrintf("SELECT channel_group_id, channel_group_name FROM t_channel_group WHERE channel_group_id=%i",&channel_group_id);
	
	if(!db->FetchRow())
		throw Exception("ChannelGroup","Unknown Channel group");
	
	this->channel_group_id = db->GetFieldInt(0);
	channel_group_name = db->GetField(1);
}

bool ChannelGroup::CheckChannelGroupName(const string &channel_group_name)
{
	int i,len;
	
	len = channel_group_name.length();
	if(len==0 || len>CHANNEL_GROUP_NAME_MAX_LEN)
		return false;
	
	for(i=0;i<len;i++)
		if(!isalnum(channel_group_name[i]) && channel_group_name[i]!='-')
			return false;
	
	return true;
}

void ChannelGroup::Get(unsigned int id, QueryResponse *response)
{
	const ChannelGroup channelgroup = ChannelGroups::GetInstance()->Get(id);
	
	response->SetAttribute("id", to_string(channelgroup.GetID()));
	response->SetAttribute("name", channelgroup.GetName());
	
	auto fields = channelgroup.fields.GetIDMap();
	for(auto it = fields.begin(); it!=fields.end(); ++it)
	{
		DOMElement node = (DOMElement)response->AppendXML("<field />");
		node.setAttribute("id",to_string(it->second.GetID()));
		node.setAttribute("name",it->second.GetName());
		node.setAttribute("type",Field::FieldTypeToString(it->second.GetType()));
	}
}

unsigned int ChannelGroup::Create(const string &name, const string &fields_str)
{
	 create_edit_check(name,fields_str);
	
	DB db("elog");
	
	db.StartTransaction();
	
	db.QueryPrintf("INSERT INTO t_channel_group(channel_group_name) VALUES(%s)", &name);
	
	unsigned int id = db.InsertID();
	
	Fields new_fields(Fields::en_type::GROUP, id);
	new_fields.Update(json::parse(fields_str));
	
	
	db.CommitTransaction();
	
	return id;
}

void ChannelGroup::Edit(unsigned int id, const std::string &name, const string &fields_str)
{
	create_edit_check(name,fields_str);
	
	if(!ChannelGroups::GetInstance()->Exists(id))
		throw Exception("ChannelGroup","Channel group not found","UNKNOWN_CHANNELGROUP");
	
	DB db("elog");
	DB db2(&db);
	
	db.StartTransaction();
	
	db.QueryPrintf("UPDATE t_channel_group SET channel_group_name=%s WHERE channel_group_id=%i", &name, &id);
	
	Fields new_fields(Fields::en_type::GROUP, id);
	new_fields.Update(json::parse(fields_str));
	
	db.CommitTransaction();
}

void ChannelGroup::Delete(unsigned int id)
{
	DB db("elog");
	
	db.StartTransaction();
	
	db.QueryPrintf("DELETE FROM t_channel_group WHERE channel_group_id=%i",&id);
	db.QueryPrintf("DELETE FROM t_channel WHERE channel_group_id=%i", &id);
	
	db.QueryPrintf("DELETE FROM t_field WHERE channel_group_id IS NOT NULL AND NOT EXISTS(SELECT * FROM t_channel_group cg WHERE cg.channel_group_id=t_field.channel_group_id)");
	db.QueryPrintf("DELETE FROM t_field WHERE channel_id IS NOT NULL AND NOT EXISTS(SELECT * FROM t_channel c WHERE c.channel_id=t_field.channel_id)");
	
	db.CommitTransaction();
}

void ChannelGroup::create_edit_check(const std::string &name, const std::string &fields)
{
	if(!CheckChannelGroupName(name))
		throw Exception("ChannelGroup","Invalid channel group name","INVALID_PARAMETER");
	
	if(fields=="")
		throw Exception("ChannelGroup","Empty fields","INVALID_PARAMETER");
	
	json j;
	
	try
	{
		j = json::parse(fields);
	}
	catch(...)
	{
		throw Exception("ChannelGroup","Invalid configuration json","INVALID_PARAMETER");
	}
}

bool ChannelGroup::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	string action = query->GetRootAttribute("action");
	
	if(action=="get")
	{
		unsigned int id = query->GetRootAttributeInt("id");
		
		Get(id,response);
		
		return true;
	}
	else if(action=="create" || action=="edit")
	{
		string name = query->GetRootAttribute("name");
		string fields = query->GetRootAttribute("fields");
		
		Events::en_types ev;
		
		if(action=="create")
		{
			unsigned int id = Create(name, fields);
			
			LoggerAPI::LogAction(user,id,"ChannelGroup",query->GetQueryGroup(),action);
			
			ev = Events::en_types::CHANNELGROUP_CREATED;
			
			response->GetDOM()->getDocumentElement().setAttribute("queue-id",to_string(id));
		}
		else
		{
			unsigned int id = query->GetRootAttributeInt("id");
			
			Edit(id,name, fields);
			
			ev = Events::en_types::CHANNELGROUP_MODIFIED;
			
			LoggerAPI::LogAction(user,id,"ChannelGroup",query->GetQueryGroup(),action);
		}
		
		ChannelGroups::GetInstance()->Reload();
		Channels::GetInstance()->Reload();
		
		Events::GetInstance()->Create(ev);
		
		return true;
	}
	else if(action=="delete")
	{
		unsigned int id = query->GetRootAttributeInt("id");
		
		Delete(id);
		
		ChannelGroups::GetInstance()->Reload();
		Channels::GetInstance()->Reload();
		
		LoggerAPI::LogAction(user,id,"ChannelGroup",query->GetQueryGroup(),action);
		
		Events::GetInstance()->Create(Events::en_types::CHANNELGROUP_REMOVED, id);
		
		return true;
	}
	
	return false;
}

}
