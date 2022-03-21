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

#include <Logs/Channel.h>
#include <Logs/Channels.h>
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

using namespace std;
using nlohmann::json;

Channel::Channel()
{
}

Channel::Channel(DB *db,unsigned int elog_channel_id)
{
	db->QueryPrintf("SELECT elog_channel_id, elog_channel_name,elog_channel_config FROM t_elog_channel WHERE elog_channel_id=%i",&elog_channel_id);
	
	if(!db->FetchRow())
		throw Exception("Channel","Unknown Channel");
	
	init(db->GetFieldInt(0), db->GetField(1), db->GetField(2));
}

Channel::Channel(unsigned int id, const std::string &name, const std::string &config)
{
	init(id, name, config);
}

void Channel::init(unsigned int id, const std::string &name, const std::string &config)
{
	this->elog_channel_id = id;
	elog_channel_name = name;
	elog_channel_config = config;
	
	try
	{
		json_config = json::parse(elog_channel_config);
	}
	catch(...)
	{
		throw Exception("Channel", "invalid config json");
	}
	
	try
	{
		log_regex = regex(json_config["regex"]);
	}
	catch(...)
	{
		throw Exception("Channel", "invalid regex");
	}
	
	crit_idx = -1;
	if(json_config["crit"].type()==nlohmann::json::value_t::string)
		crit = str_to_crit(json_config["crit"]);
	else
		crit_idx = (int)json_config["crit"];
	
	machine_idx = get_log_idx(json_config, "machine");
	domain_idx =  get_log_idx(json_config, "domain");
	ip_idx =  get_log_idx(json_config, "ip");
	uid_idx =  get_log_idx(json_config, "uid");
	status_idx =  get_log_idx(json_config, "status");
	
	if(json_config.contains("custom_fields"))
	{
		auto custom_fields = json_config["custom_fields"];
		for(auto it = custom_fields.begin(); it!=custom_fields.end(); ++it)
			custom_fields_idx[it.key()] =  get_log_idx(custom_fields, it.key());
	}
}

void Channel::ParseLog(const string log_str, map<string, string> &std_fields, map<string, string> &custom_fields)
{
	smatch matches;
	
	if(!regex_search(log_str, matches, log_regex))
		throw Exception("Channel", "unable to match log message");
	
	char buf[32];
	struct tm now_t;
	time_t now = time(0);
	localtime_r(&now, &now_t);
	strftime(buf, 32, "%Y-%m-%d %H:%M:%S", &now_t);
	
	std_fields["date"] = buf;
	
	if(crit_idx<0)
		std_fields["crit"] = crit_to_str(crit);
	else
	{
		if(crit_idx>=matches.size())
			throw Exception("StoreLog", "Unable to extract crit from log");
		
		if(str_to_crit(matches[crit_idx])>0)
			std_fields["crit"] = matches[crit_idx];
	}
	
	get_log_part(matches, "machine", machine_idx, std_fields);
	get_log_part(matches, "domain", domain_idx, std_fields);
	get_log_part(matches, "ip", ip_idx, std_fields);
	get_log_part(matches, "uid", uid_idx, std_fields);
	get_log_part(matches, "status", status_idx, std_fields);
	
	for(auto it = custom_fields_idx.begin(); it!=custom_fields_idx.end(); ++it)
		get_log_part(matches, it->first, it->second, custom_fields);
}

void Channel::get_log_part(const smatch &matches, const string &name, int idx, map<string, string> &val)
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
	
	auto e = j[name];
	if(e.type()==nlohmann::json::value_t::number_unsigned)
		return e;
	
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
	Channel channel = Channels::GetInstance()->Get(id);
	
	DOMElement node = (DOMElement)response->AppendXML("<channel />");
	node.setAttribute("id",to_string(channel.GetID()));
	node.setAttribute("name",channel.GetName());
	node.setAttribute("config",channel.GetConfig());
}

unsigned int Channel::Create(const string &name, const string &config)
{
	 create_edit_check(name,config);
	
	DB db;
	db.QueryPrintf("INSERT INTO t_elog_channel(elog_channel_name,elog_channel_config) VALUES(%s,%s)",
		&name,
		&config
	);
	
	return db.InsertID();
}

void Channel::Edit(unsigned int id, const std::string &name, const string &config)
{
	create_edit_check(name,config);
	
	if(!Channels::GetInstance()->Exists(id))
		throw Exception("Channel","Channel not found","UNKNOWN_CHANNEL");
	
	DB db;
	db.QueryPrintf("UPDATE t_elog_channel SET elog_channel_name=%s, elog_channel_config=%s WHERE elog_channel_id=%i",
		&name,
		&config,
		&id
	);
}

void Channel::Delete(unsigned int id)
{
	DB db;
	
	db.QueryPrintf("DELETE FROM t_elog_channel WHERE elog_channel_id=%i",&id);
}

int Channel::str_to_crit(const std::string &str)
{
	if(str=="LOG_EMERG")
		return 0;
	else if(str=="LOG_ALERT")
		return 1;
	else if(str=="LOG_CRIT")
		return 2;
	else if(str=="LOG_ERR")
		return 3;
	else if(str=="LOG_WARNING")
		return 4;
	else if(str=="LOG_NOTICE")
		return 5;
	else if(str=="LOG_INFO")
		return 6;
	else if(str=="LOG_DEBUG")
		return 7;
	return -1;
}

string Channel::crit_to_str(int i)
{
	switch(i)
	{
		case 0:
			return "LOG_EMERG";
		case 1:
			return "LOG_ALERT";
		case 2:
			return "LOG_CRIT";
		case 3:
			return "LOG_ERR";
		case 4:
			return "LOG_WARNING";
		case 5:
			return "LOG_NOTICE";
		case 6:
			return "LOG_INFO";
		case 7:
			return "LOG_DEBUG";
	}
	
	return "";
}

bool Channel::check_int_field(const json &j, const string &name)
{
	if(!j.contains(name))
		return true;
	
	if(j[name].type()!=nlohmann::json::value_t::number_unsigned)
		throw Exception("Channel","Attribute "+name+" must be an integer","INVALID_PARAMETER");
	
	return true;
}

void Channel::create_edit_check(const std::string &name, const std::string &config)
{
	if(!CheckChannelName(name))
		throw Exception("Channel","Invalid channel name","INVALID_PARAMETER");
	
	if(config=="")
		throw Exception("Channel","Empty config","INVALID_PARAMETER");
	
	json j;
	
	try
	{
		j = json::parse(config);
	}
	catch(...)
	{
		throw Exception("Channel","Invalid configuration json","INVALID_PARAMETER");
	}
	
	if(!j.contains("regex"))
		throw Exception("Channel","Missing regex attribute in config","INVALID_PARAMETER");
	
	string regex_str = j["regex"];
	
	if(regex_str=="")
		throw Exception("Channel","Empty regex","INVALID_PARAMETER");
	
	try
	{
		regex r(regex_str);
	}
	catch(std::regex_error &e)
	{
		throw Exception("Channel","Invalid regex : " + string(e.what()),"INVALID_PARAMETER");
	}
	
	if(!j.contains("crit"))
		throw Exception("Channel","Missing crit attribute in config","INVALID_PARAMETER");
	
	if(j["crit"].type()==nlohmann::json::value_t::string)
	{
		if(str_to_crit(j["crit"])<0)
			throw Exception("Channel","crit attribute is not a valid syslog level","INVALID_PARAMETER");
	}
	else if(j["crit"].type()!=nlohmann::json::value_t::number_unsigned)
		throw Exception("Channel","crit attribute must be an integer or a string","INVALID_PARAMETER");
	
	check_int_field(j, "machine");
	check_int_field(j, "domain");
	check_int_field(j, "ip");
	check_int_field(j, "uid");
	check_int_field(j, "status");
	
	if(j.contains("custom_fields"))
	{
		auto custom_fields = j["custom_fields"];
		
		for(auto it = custom_fields.begin(); it!=custom_fields.end(); ++it)
			check_int_field(custom_fields, it.key());
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
		string config = query->GetRootAttribute("config");
		
		if(action=="create")
		{
			unsigned int id = Create(name, config);
			
			LoggerAPI::LogAction(user,id,"Channel",query->GetQueryGroup(),action);
			
			Events::GetInstance()->Create(Events::en_types::CHANNEL_CREATED);
			
			response->GetDOM()->getDocumentElement().setAttribute("queue-id",to_string(id));
		}
		else
		{
			unsigned int id = query->GetRootAttributeInt("id");
			
			Edit(id,name, config);
			
			Events::GetInstance()->Create(Events::en_types::CHANNEL_MODIFIED);
			
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
		
		Events::GetInstance()->Create(Events::en_types::CHANNEL_REMOVED, id);
		
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
				channel = Channel(-1, "default", config);
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
