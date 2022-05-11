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

#include <ELogs/Channel.h>
#include <ELogs/Channels.h>
#include <ELogs/ChannelGroup.h>
#include <ELogs/ChannelGroups.h>
#include <ELogs/Field.h>
#include <Exception/Exception.h>
#include <DB/DB.h>
#include <Logger/Logger.h>
#include <Logger/LoggerAPI.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <User/User.h>
#include <WS/Events.h>
#include <API/QueryHandlers.h>
#include <global.h>

#include <time.h>

using namespace std;
using nlohmann::json;

namespace ELogs
{

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	qh->RegisterHandler("channel", Channel::HandleQuery);
	Events::GetInstance()->RegisterEvents({"CHANNEL_CREATED","CHANNEL_MODIFIED","CHANNEL_REMOVED"});
	return (APIAutoInit *)0;
});

Channel::Channel():fields(Fields::en_type::CHANNEL, -1)
{
}

Channel::Channel(DB *db,unsigned int channel_id):fields(Fields::en_type::CHANNEL, channel_id)
{
	db->QueryPrintf("SELECT channel_id, channel_group_id, channel_name,channel_config FROM t_channel WHERE channel_id=%i",&channel_id);
	
	if(!db->FetchRow())
		throw Exception("Channel","Unknown Channel");
	
	unsigned int id = db->GetFieldInt(0);
	unsigned int group_id = db->GetFieldInt(1);
	string name = db->GetField(2);
	string config = db->GetField(3);
	
	init(id, group_id, name, config);
}

Channel::Channel(unsigned int id, unsigned int group_id, const std::string &name, const std::string &config):fields(Fields::en_type::CHANNEL, id)
{
	init(id, group_id, name, config);
}

void Channel::init(unsigned int id, unsigned int group_id, const std::string &name, const std::string &config)
{
	const string excpt_context("Channel «"+name+"»");
	
	this->channel_id = id;
	this->channel_group_id = group_id;
	channel_name = name;
	channel_config = config;
	
	try
	{
		json_config = json::parse(channel_config);
	}
	catch(...)
	{
		throw Exception(excpt_context, "invalid config json");
	}
	
	try
	{
		log_regex = regex(json_config["regex"]);
	}
	catch(...)
	{
		throw Exception(excpt_context, "invalid regex");
	}
	
	create_edit_check(name, group_id, config);
	
	date_format = json_config["date_format"];
	if(date_format!="auto")
		date_idx = (int)json_config["date_field"];
	
	crit_idx = -1;
	if(json_config["crit"].type()==nlohmann::json::value_t::string)
		crit = Field::PackCrit(json_config["crit"]);
	else
		crit_idx = (int)json_config["crit"];
	
	auto group_matches = json_config["group_matches"];
	for(auto it = group_matches.begin(); it!=group_matches.end(); ++it)
		group_fields_idx[it.key()] = get_log_idx(group_matches, it.key());
	
	auto matches = json_config["matches"];
	for(auto it = matches.begin(); it!=matches.end(); ++it)
		fields_idx[it.key()] = get_log_idx(matches, it.key());
}

const ChannelGroup Channel::GetGroup() const
{
	return ChannelGroups::GetInstance()->Get(channel_group_id);
}

void Channel::ParseLog(const string &log_str, map<string, string> &group_fields, map<string, string> &fields) const
{
	smatch matches;
	
	if(!regex_search(log_str, matches, log_regex))
		throw Exception("Channel", "unable to match log message for channel «"+channel_name+"»");
	
	if(date_format=="auto")
	{
		// Automatic date, generate date based on reception time (now)
		char buf[32];
		struct tm now_t;
		time_t now = time(0);
		localtime_r(&now, &now_t);
		strftime(buf, 32, "%Y-%m-%d %H:%M:%S", &now_t);
		
		group_fields["date"] = buf;
	}
	else
	{
		// Parse date, based on specified format
		struct tm date_t;
		memset(&date_t, 0, sizeof(struct tm));
		
		// Get raw date and parse it
		get_log_part(matches, "date", date_idx, group_fields);
		if(!strptime(group_fields["date"].c_str(), date_format.c_str(), &date_t))
			throw Exception("Channel", "Unable to parse date");
		
		// Normalize date
		char buf[32];
		strftime(buf, 32, "%Y-%m-%d %H:%M:%S", &date_t);
		group_fields["date"] = buf;
		
	}
	
	if(crit_idx<0)
		group_fields["crit"] = Field::UnpackCrit(crit);
	else
	{
		if(crit_idx>=matches.size())
			throw Exception("Channel", "Unable to extract crit from log");
		
		if(Field::PackCrit(matches[crit_idx])>0)
			group_fields["crit"] = matches[crit_idx];
	}
	
	for(auto it = group_fields_idx.begin(); it!=group_fields_idx.end(); ++it)
		get_log_part(matches, it->first, it->second, group_fields);
	
	for(auto it = fields_idx.begin(); it!=fields_idx.end(); ++it)
		get_log_part(matches, it->first, it->second, fields);
}

void Channel::get_log_part(const smatch &matches, const string &name, int idx, map<string, string> &val) const
{
	if(idx<0)
		return;
	
	if(idx>matches.size()-1)
		throw Exception("Channel", "could not extract '"+name+"' field as group "+to_string(idx)+" has not been matched");
	
	val[name] = matches[idx];
}

int Channel::get_log_idx(const json &j, const string &name)
{
	if(!j.contains(name))
		return -1;
	
	if(j[name].type()==nlohmann::json::value_t::number_unsigned)
		return j[name];
	
	return -1;
}

bool Channel::CheckChannelName(const string &channel_name)
{
	int i,len;
	
	len = channel_name.length();
	if(len==0 || len>CHANNEL_NAME_MAX_LEN)
		return false;
	
	for(i=0;i<len;i++)
		if(!isalnum(channel_name[i]) && channel_name[i]!='_' && channel_name[i]!='-')
			return false;
	
	return true;
}

void Channel::Get(unsigned int id, QueryResponse *response)
{
	const Channel channel = Channels::GetInstance()->Get(id);
	
	response->SetAttribute("id",to_string(channel.GetID()));
	response->SetAttribute("group_id",to_string(channel.GetGroupID()));
	response->SetAttribute("name",channel.GetName());
	response->SetAttribute("config",channel.GetConfig());
	
	channel.GetFields().AppendXMLDescription(response, response->GetDOM()->getDocumentElement());
}

unsigned int Channel::Create(const string &name, unsigned int group_id, const string &config)
{
	 create_edit_check(name, group_id, config);
	
	DB db("elog");
	db.QueryPrintf("INSERT INTO t_channel(channel_name,channel_group_id, channel_config) VALUES(%s,%i,%s)",
		&name,
		&group_id,
		&config
	);
	
	unsigned int id = db.InsertID();
	
	json j = json::parse(config);
	if(j.contains("fields"))
	{
		Fields new_fields(Fields::en_type::CHANNEL, id);
		new_fields.Update(j["fields"]);
	}
	
	return id;
}

void Channel::Edit(unsigned int id, const std::string &name, unsigned int group_id, const string &config)
{
	create_edit_check(name, group_id, config);
	
	if(!Channels::GetInstance()->Exists(id))
		throw Exception("Channel","Channel not found","UNKNOWN_CHANNEL");
	
	DB db("elog");
	db.QueryPrintf("UPDATE t_channel SET channel_name=%s, channel_group_id=%i, channel_config=%s WHERE channel_id=%i",
		&name,
		&group_id,
		&config,
		&id
	);
	
	json j = json::parse(config);
	if(j.contains("fields"))
	{
		Fields new_fields(Fields::en_type::CHANNEL, id);
		new_fields.Update(j["fields"]);
	}
}

void Channel::Delete(unsigned int id)
{
	DB db("elog");
	
	db.StartTransaction();
	
	db.QueryPrintf("DELETE FROM t_channel WHERE channel_id=%i",&id);
	db.QueryPrintf("DELETE FROM t_field WHERE channel_id IS NOT NULL AND NOT EXISTS(SELECT * FROM t_channel c WHERE c.channel_id=t_field.channel_id)");
	
	db.CommitTransaction();
}

bool Channel::check_int_field(const json &j, const string &name)
{
	if(!j.contains(name))
		return true;
	
	if(j[name].type()!=nlohmann::json::value_t::number_unsigned)
		throw Exception("Channel","Attribute "+name+" must be an integer","INVALID_PARAMETER");
	
	return true;
}

void Channel::create_edit_check(const std::string &name, unsigned int group_id, const std::string &config)
{
	const string excpt_context("Channel «"+name+"»");
	
	if(!CheckChannelName(name))
		throw Exception(excpt_context,"Invalid channel name","INVALID_PARAMETER");
	
	if(group_id!=-1)
		ChannelGroups::GetInstance()->Get(group_id);
	
	if(config=="")
		throw Exception(excpt_context,"Empty config","INVALID_PARAMETER");
	
	json j;
	
	try
	{
		j = json::parse(config);
	}
	catch(...)
	{
		throw Exception(excpt_context,"Invalid configuration json","INVALID_PARAMETER");
	}
	
	if(!j.contains("regex"))
		throw Exception(excpt_context,"Missing regex attribute in config","INVALID_PARAMETER");
	
	string regex_str = j["regex"];
	
	if(regex_str=="")
		throw Exception(excpt_context,"Empty regex","INVALID_PARAMETER");
	
	try
	{
		regex r(regex_str);
	}
	catch(std::regex_error &e)
	{
		throw Exception(excpt_context,"Invalid regex : " + string(e.what()),"INVALID_PARAMETER");
	}
	
	if(!j.contains("date_format"))
		throw Exception(excpt_context,"Missing date_format attribute in config","INVALID_PARAMETER");
	
	if(j["date_format"].type()!=nlohmann::json::value_t::string || j["date_format"]=="")
		throw Exception(excpt_context,"date_format attribute must be a non empty string","INVALID_PARAMETER");
	
	// Check date format / field
	if(j["date_format"]!="auto")
	{
		if(!j.contains("date_field"))
			throw Exception(excpt_context,"date_field must be provided if date format is not automatic","INVALID_PARAMETER");
	
		check_int_field(j, "date_field");
	}
	
	if(!j.contains("crit"))
		throw Exception(excpt_context,"Missing crit attribute in config","INVALID_PARAMETER");
	
	if(j["crit"].type()==nlohmann::json::value_t::string)
		Field::PackCrit(j["crit"]);
	else if(j["crit"].type()!=nlohmann::json::value_t::number_unsigned)
		throw Exception(excpt_context,"crit attribute must be an integer or a string","INVALID_PARAMETER");
	
	if(j.contains("group_matches"))
	{
		auto group_matches = j["group_matches"];
		
		for(auto it = group_matches.begin(); it!=group_matches.end(); ++it)
			check_int_field(group_matches, it.key());
	}
	
	if(j.contains("matches"))
	{
		auto matches = j["matches"];
		
		// Check group and channel fields
		for(auto it = matches.begin(); it!=matches.end(); ++it)
			check_int_field(matches, it.key());
	}
}

bool Channel::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
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
		unsigned int group_id = query->GetRootAttributeInt("group_id");
		string config = query->GetRootAttribute("config");
		
		if(action=="create")
		{
			unsigned int id = Create(name, group_id, config);
			
			LoggerAPI::LogAction(user,id,"Channel",query->GetQueryGroup(),action);
			
			Events::GetInstance()->Create("CHANNEL_CREATED");
			
			response->GetDOM()->getDocumentElement().setAttribute("channel-id",to_string(id));
		}
		else
		{
			unsigned int id = query->GetRootAttributeInt("id");
			
			Edit(id,name, group_id, config);
			
			Events::GetInstance()->Create("CHANNEL_MODIFIED");
			
			LoggerAPI::LogAction(user,id,"Channel",query->GetQueryGroup(),action);
		}
		
		Channels::GetInstance()->Reload();
		
		return true;
	}
	else if(action=="delete")
	{
		unsigned int id = query->GetRootAttributeInt("id");
		
		Delete(id);
		
		Channels::GetInstance()->Reload();
		
		LoggerAPI::LogAction(user,id,"Channel",query->GetQueryGroup(),action);
		
		Events::GetInstance()->Create("CHANNEL_REMOVED", id);
		
		return true;
	}
	else if(action=="checkregex")
	{
		try
		{
			string regex_str = query->GetRootAttribute("regex");
			regex r(regex_str);
			
			response->SetAttribute("valid", "yes");
		}
		catch(std::regex_error &e)
		{
			response->SetAttribute("valid", "no");
			response->SetAttribute("details", e.what());
		}
		
		return true;
	}
	else if(action=="testlog")
	{
		int id = query->GetRootAttributeInt("id", -1);
		int group_id = query->GetRootAttributeInt("group_id", -1);
		string config = query->GetRootAttribute("config", "");
		string log = query->GetRootAttribute("log");
		
		if(id==-1 && config=="")
			throw Exception("Channel","id and config cannot be both empty","INVALID_PARAMETER");
		
		map<string, string> std, custom;
		
		Channel channel;
		
		try
		{
			if(id!=-1)
				channel = Channels::GetInstance()->Get(id);
			if(config!="")
				channel = Channel(-1, group_id, "default", config);
		}
		catch(Exception &e)
		{
			response->SetAttribute("valid", "no");
			response->SetAttribute("details", e.error);
			return true;
		}
		
		try
		{
			channel.ParseLog(log, std, custom);
			
			json j;
			
			for(auto it=std.begin(); it!=std.end(); ++it)
				j["std"][it->first] = it->second;
			
			for(auto it=custom.begin(); it!=custom.end(); ++it)
				j["custom"][it->first] = it->second;
			
			response->SetAttribute("valid", "yes");
			response->SetAttribute("fields", j.dump());
		}
		catch(Exception &e)
		{
			response->SetAttribute("valid", "no");
			response->SetAttribute("details", e.error);
		}
		
		return true;
	}
	
	return false;
}

}
