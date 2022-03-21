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

#include <Logs/LogStorage.h>
#include <Exception/Exception.h>
#include <DB/DB.h>
#include <Logger/Logger.h>
#include <WS/Events.h>
#include <Logs/Channels.h>
#include <Logs/Channel.h>
#include <Configuration/ConfigurationEvQueue.h>

#include <arpa/inet.h>

#include <vector>

#include <nlohmann/json.hpp>

#define FIELD_TYPE_INT     1
#define FIELD_TYPE_STR     2
#define FIELD_TYPE_IP      3
#define FIELD_TYPE_CRIT    4
#define FIELD_TYPE_PACK    5

using namespace std;
using nlohmann::json;

LogStorage *LogStorage::instance = 0;

LogStorage::LogStorage()
{
	DB db;
	
	Configuration *config = ConfigurationEvQueue::GetInstance();
	max_queue_size = config->GetInt("elog.queue.size");
	
	db.Query("SELECT elog_pack_id, elog_pack_string FROM t_elog_pack");
	while(db.FetchRow())
		pack_str_id[db.GetField(1)] = db.GetFieldInt(0);
	
	instance = this;
	
	ls_thread_handle = thread(LogStorage::ls_thread,this);
}

void LogStorage::Shutdown(void)
{
	is_shutting_down = true;
	
	Log("");
}

void LogStorage::WaitForShutdown(void)
{
	ls_thread_handle.join();
}

void *LogStorage::ls_thread(LogStorage *ls)
{
	DB::StartThread();
	
	Logger::Log(LOG_NOTICE,"Log storage started");
	
	unique_lock<mutex> llock(ls->lock);
	
	while(true)
	{
		if(ls->logs.empty())
			ls->logs_queued.wait(llock);
		
		if(ls->is_shutting_down)
		{
			Logger::Log(LOG_NOTICE,"Shutdown in progress exiting Log storage");
			
			DB::StopThread();
			
			return 0;
		}
		
		string log = ls->logs.front();
		ls->logs.pop();
		
		llock.unlock();
		
		ls->log(log);
		
		llock.lock();
	}
}

void LogStorage::Log(const std::string &str)
{
	unique_lock<mutex> llock(lock);
	
	if(logs.size()>=max_queue_size)
	{
		Logger::Log(LOG_WARNING,"External logs queue size is full, discarding log");
		return;
	}
		
	
	logs.push(str);
	logs_queued.notify_one();
}

void LogStorage::log(const std::string &str)
{
	regex r("([a-zA-Z0-9_-]+)[ ]+");
	
	smatch matches;
	
	try
	{
		if(!regex_search(str, matches, r))
			throw Exception("LogStorage", "unable to get log message channel");
		
		if(matches.size()!=2)
			throw Exception("LogStorage", "unable to get log message channel");
		
		string channel_name = matches[1];
		string log_str = str.substr(matches[0].length());
		
		Channel channel = Channels::GetInstance()->Get(channel_name);
		
		map<string, string> std, custom;
		channel.ParseLog(log_str, std, custom);
		
		store_log(channel.GetID(), std, custom);
	}
	catch(Exception &e)
	{
		Logger::Log(LOG_ERR, "Error parsing extern log in "+e.context+" : "+e.error);
	}
}

void LogStorage::store_log(unsigned int channel_id, map<string, string> &std_fields, map<string, string> &custom_fields)
{
	string query = "INSERT INTO t_elog(elog_channel_id, elog_date, elog_crit, elog_machine, elog_domain, elog_ip, elog_uid, elog_status, elog_fields) VALUES(";
	
	vector<void *> vals;
	string date, ip, uid, custom;
	int crit, machine, domain, status;
	
	
	query += "%i";
	vals.push_back(&channel_id);
	
	add_query_field(FIELD_TYPE_STR, query, "date", std_fields, &date, vals);
	add_query_field(FIELD_TYPE_CRIT, query, "crit", std_fields, &crit, vals);
	add_query_field(FIELD_TYPE_PACK, query, "machine", std_fields, &machine, vals);
	add_query_field(FIELD_TYPE_PACK, query, "domain", std_fields, &domain, vals);
	add_query_field(FIELD_TYPE_IP, query, "ip", std_fields, &ip, vals);
	add_query_field(FIELD_TYPE_STR, query, "uid", std_fields, &uid, vals);
	add_query_field(FIELD_TYPE_INT, query, "status", std_fields, &status, vals);
	
	if(custom_fields.size()>0)
	{
		json j;
		for(auto it=custom_fields.begin(); it!=custom_fields.end(); ++it)
			j[it->first] = it->second;
		
		custom = j.dump();
		query += ", %s";
		vals.push_back(&custom);
	}
	else
		query += ", NULL";
	
	query += ")";
	
	DB db;
	try
	{
		db.QueryVsPrintf(query, vals);
	}
	catch(Exception &e)
	{
		if(e.codeno==1526) // No partition for data
		{
			// Automatically create partition to store new logs
			db.QueryPrintf("SELECT TO_DAYS(%s)", &date);
			db.FetchRow();
			
			int days = db.GetFieldInt(0)+1;
			string part_name = "p"+date.substr(0, 4)+date.substr(5, 2)+date.substr(8, 2);
			
			db.QueryPrintf("ALTER TABLE t_elog ADD PARTITION (PARTITION "+part_name+" VALUES LESS THAN (%i))", &days);
		}
	}
	
	Events::GetInstance()->Create(Events::en_types::LOG_ELOG);
}

void LogStorage::add_query_field(int type, string &query, const string &name, map<string, string> &fields, void *val, vector<void *> &vals)
{
	auto it = fields.find(name);
	if(it==fields.end())
	{
		query += ", NULL";
		return;
	}
	
	if(type==FIELD_TYPE_INT)
	{
		query += ", %i";
		*(int*)val = PackInteger(it->second);
	}
	else if(type==FIELD_TYPE_STR)
	{
		query += ", %s";
		*(string*)val = it->second;
	}
	else if(type==FIELD_TYPE_IP)
	{
		query += ", %s";
		*(string*)val = PackIP(it->second);
	}
	else if(type==FIELD_TYPE_CRIT)
	{
		query += ", %i";
		*(int*)val = PackCrit(it->second);
	}
	if(type==FIELD_TYPE_PACK)
	{
		query += ", %i";
		*(int*)val = PackString(it->second);
	}
	
	vals.push_back(val);
}

int LogStorage::PackInteger(const string &str)
{
	try
	{
		return stoi(str);
	}
	catch(...)
	{
		throw Exception("LogStorage", "Invalid integer : "+str);
	}
}

string LogStorage::UnpackInteger(int i)
{
	return to_string(i);
}

string LogStorage::PackIP(const string &ip)
{
	char bin[16];
	
	if(inet_pton(AF_INET, ip.c_str(), bin))
		return string(bin, 4);
	
	if(inet_pton(AF_INET6, ip.c_str(), bin))
		return string(bin, 16);
	
	throw Exception("LogStorage", "Invalid IP : "+ip);
}

string LogStorage::UnpackIP(const string &bin_ip)
{
	char buf[64];
	
	if(bin_ip.size()==4)
	{
		if(!inet_ntop(AF_INET, bin_ip.c_str(), buf, 64))
			return "";
		
		return buf;
	}
	
	if(bin_ip.size()==16)
	{
		if(!inet_ntop(AF_INET6, bin_ip.c_str(), buf, 64))
			return "";
		
		return buf;
	}
	
	throw Exception("LogStorage", "Invalid binary IP length should be 4 ou 16");
}

int LogStorage::PackCrit(const string &str)
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

string LogStorage::UnpackCrit(int i)
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

unsigned int LogStorage::PackString(const string &str)
{
	unique_lock<mutex> llock(lock);
	
	auto it = pack_str_id.find(str);
	
	if(it!=pack_str_id.end())
		return it->second;
	
	DB db;
	db.QueryPrintf("REPLACE INTO t_elog_pack(elog_pack_string) VALUES(%s)", &str);
	
	unsigned int id = db.InsertID();
	pack_str_id[str] = id;
	
	return id;
}

