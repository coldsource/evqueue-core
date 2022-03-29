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
#include <Logs/ChannelGroup.h>
#include <Configuration/ConfigurationEvQueue.h>

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

LogStorage::LogStorage(): channel_regex("([a-zA-Z0-9_-]+)[ ]+")
{
	DB db("elog");
	
	Configuration *config = ConfigurationEvQueue::GetInstance();
	max_queue_size = config->GetInt("elog.queue.size");
	bulk_size = config->GetInt("elog.bulk.size");
	
	db.Query("SELECT pack_id, pack_string FROM t_pack");
	while(db.FetchRow())
	{
		pack_str_id[db.GetField(1)] = db.GetFieldInt(0);
		pack_id_str[db.GetFieldInt(0)] = db.GetField(1);
	}
	
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
	
	vector<string> logs;
	
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
		
		logs.clear();
		
		for(int i=0;i<ls->bulk_size && !ls->logs.empty();i++)
		{
			logs.push_back(ls->logs.front());
			ls->logs.pop();
		}
		
		llock.unlock();
		
		ls->log(logs);
		
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

void LogStorage::log(const vector<string> &logs)
{
	smatch matches;
	
	DB db("elog");
	
	db.StartTransaction();
	
	for(int i=0;i<logs.size();i++)
	{
		try
		{
			if(!regex_search(logs[i], matches, channel_regex))
				throw Exception("LogStorage", "unable to get log message channel");
			
			if(matches.size()!=2)
				throw Exception("LogStorage", "unable to get log message channel");
			
			string channel_name = matches[1];
			string log_str = logs[i].substr(matches[0].length());
			
			const Channel channel = Channels::GetInstance()->Get(channel_name);
			
			map<string, string> group_fields, channel_fields;
			channel.ParseLog(log_str, group_fields, channel_fields);
			
			store_log(&db, channel, group_fields, channel_fields);
		}
		catch(Exception &e)
		{
			Logger::Log(LOG_ERR, "Error parsing extern log in "+e.context+" : "+e.error);
		}
	}
	
	db.CommitTransaction();
}

void LogStorage::store_log(DB *db, const Channel &channel, const map<string, string> &group_fields, const map<string, string> &channel_fields)
{
	const ChannelGroup group = channel.GetGroup();
	
	// Insert log line
	unsigned int channel_id = channel.GetID();
	const string date = group_fields.find("date")->second;
	int crit = Field::PackCrit(group_fields.find("crit")->second);
	
	try
	{
		printf("insert\n");
		db->QueryPrintf("INSERT INTO t_log(channel_id, log_date, log_crit) VALUES(%i, %s, %i)", &channel_id, &date, &crit);
		printf("insert done\n");
	}
	catch(Exception &e)
	{
		if(e.codeno==1526) // No partition for data
		{
			// Automatically create partition to store new logs
			db->QueryPrintf("SELECT TO_DAYS(%s)", &date);
			db->FetchRow();
			
			int days = db->GetFieldInt(0)+1;
			string part_name = "p"+date.substr(0, 4)+date.substr(5, 2)+date.substr(8, 2);
			
			db->QueryPrintf("ALTER TABLE t_log ADD PARTITION (PARTITION "+part_name+" VALUES LESS THAN (%i))", &days);
			
			db->QueryPrintf("INSERT INTO t_log(channel_id, log_date, log_crit) VALUES(%i, %s, %i)", &channel_id, &date, &crit);
		}
	}
	
	unsigned long long id = db->InsertIDLong();
	
	printf("log value\n");
	for(auto it = group_fields.begin(); it!=group_fields.end(); ++it)
	{
		if(it->first=="date" || it->first=="crit")
			continue;
		
		log_value(db, id, group.GetFields().GetField(it->first), date, it->second);
	}
	
	for(auto it = channel_fields.begin(); it!=channel_fields.end(); ++it)
		log_value(db, id, channel.GetFields().GetField(it->first), date, it->second);
	printf("done\n");
	
	Events::GetInstance()->Create(Events::en_types::LOG_ELOG);
}

void LogStorage::log_value(DB *db, unsigned long long log_id, const Field &field, const string &date, const string &value)
{
	unsigned int field_id = field.GetID();
	
	const string table_name = field.GetTableName();
	
	int val_int;
	string val_str;
	void *packed_val = field.Pack(value, &val_int, &val_str);
	
	db->QueryPrintf("INSERT INTO %c(log_id, field_id, log_date, value) VALUES(%l, %i, %s, %s)", &table_name, &log_id, &field_id, &date, packed_val);
}

unsigned int LogStorage::PackString(const string &str)
{
	unique_lock<mutex> llock(lock);
	
	auto it = pack_str_id.find(str);
	
	if(it!=pack_str_id.end())
		return it->second;
	
	DB db("elog");
	db.QueryPrintf("REPLACE INTO t_pack(pack_string) VALUES(%s)", &str);
	
	unsigned int id = db.InsertID();
	pack_str_id[str] = id;
	pack_id_str[id] = str;
	
	return id;
}

string LogStorage::UnpackString(int i)
{
	unique_lock<mutex> llock(lock);
	
	auto it = pack_id_str.find(i);
	
	if(it!=pack_id_str.end())
		return it->second;
	
	return "";
}
