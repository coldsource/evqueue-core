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

#include <ELogs/LogStorage.h>
#include <Exception/Exception.h>
#include <DB/DB.h>
#include <Logger/Logger.h>
#include <WS/Events.h>
#include <ELogs/Channels.h>
#include <ELogs/Channel.h>
#include <ELogs/ChannelGroup.h>
#include <Configuration/Configuration.h>
#include <Crypto/Sha1String.h>
#include <API/QueryHandlers.h>
#include <IO/NetworkConnections.h>

#include <vector>

#include <nlohmann/json.hpp>

#define FIELD_TYPE_INT     1
#define FIELD_TYPE_STR     2
#define FIELD_TYPE_IP      3
#define FIELD_TYPE_CRIT    4
#define FIELD_TYPE_PACK    5

using namespace std;
using nlohmann::json;

namespace ELogs
{

LogStorage *LogStorage::instance = 0;

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	if(!Configuration::GetInstance()->GetBool("elog.enable"))
		return (APIAutoInit *)0;
	
	// Create UDP socket
	Configuration *config = Configuration::GetInstance();
	NetworkConnections *nc = NetworkConnections::GetInstance();
	
	if(config->Get("elog.bind.ip")!="")
	{
		nc->RegisterUDP("ELogs (udp)", config->Get("elog.bind.ip"), config->GetInt("elog.bind.port"), config->GetSize("elog.log.maxsize"), [](char *buf, size_t len) {
			LogStorage::GetInstance()->Log(string(buf, len));
		});
	}
	
	Events::GetInstance()->RegisterEvent("LOG_ELOG");
	
	return (APIAutoInit *)new LogStorage();
});

LogStorage::LogStorage(): ConsumerThread(this), channel_regex("([a-zA-Z0-9_-]+)[ ]+")
{
	DB db("elog");
	storage_db = new DB("elog");
	
	Configuration *config = Configuration::GetInstance();
	max_queue_size = config->GetInt("elog.queue.size");
	bulk_size = config->GetInt("elog.bulk.size");
	
	db.Query("SELECT pack_id, pack_string FROM t_pack");
	while(db.FetchRow())
	{
		pack_str_id[db.GetField(1)] = db.GetFieldInt(0);
		pack_id_str[db.GetFieldInt(0)] = db.GetField(1);
	}
	
	string dbname = config->Get("elog.mysql.database");
	db.QueryPrintf(
		"SELECT PARTITION_DESCRIPTION FROM information_schema.partitions WHERE TABLE_SCHEMA=%s AND TABLE_NAME = 't_log' AND PARTITION_NAME IS NOT NULL ORDER BY PARTITION_DESCRIPTION DESC LIMIT 1",
		{&dbname}
	);
	
	if(db.FetchRow())
		last_partition_days = db.GetFieldInt(0);
	
	instance = this;
	
	start(); // Start consumer thread
}

LogStorage::~LogStorage()
{
	Shutdown();
	
	delete storage_db;
}

bool LogStorage::data_available()
{
	return logs.size()>0;
}

void LogStorage::init_thread()
{
	DB::StartThread();
	
	Logger::Log(LOG_NOTICE,"Log storage started");
}

void LogStorage::release_thread()
{
	Logger::Log(LOG_NOTICE,"Shutdown in progress exiting Log storage");
	
	DB::StopThread();
}

void LogStorage::get()
{
	LogStorage *ls = (LogStorage *)producer;
	
	to_insert_logs.clear();
		
	for(int i=0;i<ls->bulk_size && !ls->logs.empty();i++)
	{
		to_insert_logs.push_back(ls->logs.front());
		ls->logs.pop();
	}
}

void LogStorage::process()
{
	LogStorage *ls = (LogStorage *)producer;
	
	try
	{
		ls->log(to_insert_logs);
	}
	catch(Exception &e)
	{
		Logger::Log(LOG_ERR,"Unexpected exception in log storage ("+e.context+") : "+e.error);
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
	produced();
}

void LogStorage::log(const vector<string> &logs)
{
	smatch matches;
	
	// Compute logs id range
	int nlogs = logs.size();
	storage_db->QueryPrintf("UPDATE t_seq SET seq_value=last_insert_id(seq_value + %i) where seq_name='log_id'",{&nlogs});
	next_log_id = storage_db->InsertIDLong() - nlogs;
	
	storage_db->BulkStart(Field::en_type::NONE, "t_log", "log_id, channel_id, log_date, log_crit", 4);
	storage_db->BulkStart(Field::en_type::CHAR, "t_value_char", "log_id, field_id, log_date, value", 4);
	storage_db->BulkStart(Field::en_type::INT, "t_value_int", "log_id, field_id, log_date, value", 4);
	storage_db->BulkStart(Field::en_type::IP, "t_value_ip", "log_id, field_id, log_date, value", 4);
	storage_db->BulkStart(Field::en_type::PACK, "t_value_pack", "log_id, field_id, log_date, value", 4);
	storage_db->BulkStart(Field::en_type::TEXT, "t_value_text", "log_id, field_id, log_date, value", 4);
	storage_db->BulkStart(Field::en_type::ITEXT, "t_value_itext", "log_id, field_id, log_date, value, value_sha1", 5);
	
	for(int i=0;i<logs.size();i++)
	{
		string log_str;
		
		try
		{
			if(!regex_search(logs[i], matches, channel_regex))
				throw Exception("LogStorage", "unable to get log message channel");
			
			if(matches.size()!=2)
				throw Exception("LogStorage", "unable to get log message channel");
			
			string channel_name = matches[1];
			log_str = logs[i].substr(matches[0].length());
			
			const Channel channel = Channels::GetInstance()->Get(channel_name);
			
			map<string, string> group_fields, channel_fields;
			channel.ParseLog(log_str, group_fields, channel_fields);
			
			store_log(channel, group_fields, channel_fields);
		}
		catch(Exception &e)
		{
			Logger::Log(LOG_ERR, "Error parsing extern log in "+e.context+" : "+e.error+". Log is : "+log_str);
		}
	}
	
	storage_db->StartTransaction();
	
	try
	{
		storage_db->BulkExec(Field::en_type::NONE);
		storage_db->BulkExec(Field::en_type::CHAR);
		storage_db->BulkExec(Field::en_type::INT);
		storage_db->BulkExec(Field::en_type::IP);
		storage_db->BulkExec(Field::en_type::PACK);
		storage_db->BulkExec(Field::en_type::TEXT);
		storage_db->BulkExec(Field::en_type::ITEXT);
	}
	catch(Exception &e)
	{
		Logger::Log(LOG_ERR, "Error parsing extern log in "+e.context+" : "+e.error);
	}
	
	storage_db->CommitTransaction();
	
	Events::GetInstance()->Create("LOG_ELOG");
}

void LogStorage::store_log(const Channel &channel, const map<string, string> &group_fields, const map<string, string> &channel_fields)
{
	const ChannelGroup group = channel.GetGroup();
	unsigned long long log_id = next_log_id++;
	string date = group_fields.find("date")->second;
	
	// Check if partition exists for the log line
	if(DB::TO_DAYS(date)>=last_partition_days)
		create_partition(date);
	
	// Insert log line
	storage_db->BulkDataLong(Field::en_type::NONE, log_id);
	storage_db->BulkDataInt(Field::en_type::NONE, channel.GetID());
	storage_db->BulkDataString(Field::en_type::NONE, date);
	storage_db->BulkDataInt(Field::en_type::NONE, Field::PackCrit(group_fields.find("crit")->second));
	
	
	for(auto it = group_fields.begin(); it!=group_fields.end(); ++it)
	{
		if(it->first=="date" || it->first=="crit")
			continue;
		
		log_value(log_id, group.GetFields().Get(it->first), date, it->second);
	}
	
	for(auto it = channel_fields.begin(); it!=channel_fields.end(); ++it)
		log_value(log_id, channel.GetFields().Get(it->first), date, it->second);
}

void LogStorage::log_value(unsigned long long log_id, const Field &field, const string &date, const string &value)
{
	auto type = field.GetType();
	
	int pack_i;
	string pack_str;
	field.Pack(value, &pack_i, &pack_str);
	
	storage_db->BulkDataLong(type, log_id);
	storage_db->BulkDataInt(type, field.GetID());
	storage_db->BulkDataString(type, date);
	if(field.GetType()==Field::en_type::ITEXT)
	{
		storage_db->BulkDataString(type, pack_str.substr(0, 65535));
		storage_db->BulkDataString(type, Sha1String(pack_str).GetBinary());
	}
	else if(field.GetType()==Field::en_type::TEXT)
		storage_db->BulkDataString(type, pack_str.substr(0, 65535));
	else if(field.GetType()==Field::en_type::CHAR)
		storage_db->BulkDataString(type, pack_str.substr(0, 128));
	else if(field.GetDBType()=="%i")
		storage_db->BulkDataInt(type, pack_i);
	else if(field.GetDBType()=="%s")
		storage_db->BulkDataString(type, pack_str);
}

unsigned int LogStorage::PackString(const string &str)
{
	unique_lock<mutex> llock(lock);
	
	auto it = pack_str_id.find(str);
	
	if(it!=pack_str_id.end())
		return it->second;
	
	DB db("elog");
	db.QueryPrintf("REPLACE INTO t_pack(pack_string) VALUES(%s)", {&str});
	
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
	
	// Try to load value from database if not in cache
	{
		DB db("elog");

		db.QueryPrintf("SELECT pack_string FROM t_pack WHERE pack_id=%i", {&i});
		if(db.FetchRow())
		{
			pack_id_str[i] = db.GetField(0);
			pack_str_id[db.GetField(0)] = i;
			return db.GetField(0);
		}
	}
	return "";
}

void LogStorage::create_partition(const string &date)
{
	string part_name = "p"+date.substr(0, 4)+date.substr(5, 2)+date.substr(8, 2);
	unsigned int days = DB::TO_DAYS(date) + 1;
	
	if(days<=last_partition_days)
		return; // Nothing to do
	
	storage_db->QueryPrintf("ALTER TABLE t_log ADD PARTITION (PARTITION "+part_name+" VALUES LESS THAN (%i))", {&days});
	storage_db->QueryPrintf("ALTER TABLE t_value_char ADD PARTITION (PARTITION "+part_name+" VALUES LESS THAN (%i))", {&days});
	storage_db->QueryPrintf("ALTER TABLE t_value_text ADD PARTITION (PARTITION "+part_name+" VALUES LESS THAN (%i))", {&days});
	storage_db->QueryPrintf("ALTER TABLE t_value_itext ADD PARTITION (PARTITION "+part_name+" VALUES LESS THAN (%i))", {&days});
	storage_db->QueryPrintf("ALTER TABLE t_value_ip ADD PARTITION (PARTITION "+part_name+" VALUES LESS THAN (%i))", {&days});
	storage_db->QueryPrintf("ALTER TABLE t_value_int ADD PARTITION (PARTITION "+part_name+" VALUES LESS THAN (%i))", {&days});
	storage_db->QueryPrintf("ALTER TABLE t_value_pack ADD PARTITION (PARTITION "+part_name+" VALUES LESS THAN (%i))", {&days});
	
	last_partition_days = days;
}

}
