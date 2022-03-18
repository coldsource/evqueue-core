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
	
	db.Query("SELECT elog_pack_id, elog_pack_string FROM t_elog_pack");
	while(db.FetchRow())
		pack_str_id[db.GetField(1)] = db.GetFieldInt(0);
	
	instance = this;
}

void LogStorage::StoreLog(unsigned int channel_id, map<string, string> &std_fields, map<string, string> &custom_fields)
{
	string query = "INSERT INTO t_elog(elog_channel_id, elog_date, elog_crit, elog_machine, elog_domain, elog_ip, elog_uid, elog_status, elog_fields) VALUES(";
	
	vector<void *> vals;
	
	query += "%i";
	vals.push_back(&channel_id);
	
	string date;
	add_query_field(FIELD_TYPE_STR, query, "date", std_fields, &date, vals);
	
	int crit;
	add_query_field(FIELD_TYPE_CRIT, query, "crit", std_fields, &crit, vals);
	
	int machine;
	add_query_field(FIELD_TYPE_PACK, query, "machine", std_fields, &machine, vals);
	
	int domain;
	add_query_field(FIELD_TYPE_PACK, query, "domain", std_fields, &domain, vals);
	
	string ip;
	add_query_field(FIELD_TYPE_IP, query, "ip", std_fields, &ip, vals);
	
	string uid;
	add_query_field(FIELD_TYPE_STR, query, "uid", std_fields, &uid, vals);
	
	int status;
	add_query_field(FIELD_TYPE_INT, query, "status", std_fields, &status, vals);
	
	string custom;
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
	db.QueryVsPrintf(query, vals);
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
	std::unique_lock<std::mutex> llock(lock);
	
	auto it = pack_str_id.find(str);
	
	if(it!=pack_str_id.end())
		return it->second;
	
	DB db;
	db.QueryPrintf("REPLACE INTO t_elog_pack(elog_pack_string) VALUES(%s)", &str);
	
	unsigned int id = db.InsertID();
	pack_str_id[str] = id;
	
	return id;
}

