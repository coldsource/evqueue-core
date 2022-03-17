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

#include <vector>

#include <nlohmann/json.hpp>

using namespace std;
using nlohmann::json;

LogStorage::LogStorage()
{
	DB db;
	
	db.Query("SELECT elog_pack_id, elog_pack_string FROM t_elog_pack");
	while(db.FetchRow())
		pack_str_id[db.GetField(1)] = db.GetFieldInt(0);
}

void LogStorage::StoreLog(unsigned int channel_id, map<string, string> &std_fields, map<string, string> &custom_fields)
{
	std::unique_lock<std::mutex> llock(lock);
	
	string query = "INSERT INTO t_elog(elog_channel_id, elog_date, elog_crit, elog_machine, elog_domain, elog_ip, elog_uid, elog_status, elog_fields) VALUES(";
	
	vector<void *> vals;
	
	query += "%i";
	vals.push_back(&channel_id);
	
	string date;
	add_query_field_str(query, "date", std_fields, &date, vals);
	
	int crit;
	add_query_field_int(query, "crit", std_fields, &crit, vals, true);
	
	int machine;
	add_query_field_int(query, "machine", std_fields, &machine, vals, true);
	
	int domain;
	add_query_field_int(query, "domain", std_fields, &domain, vals, true);
	
	string ip;
	add_query_field_str(query, "ip", std_fields, &ip, vals);
	
	string uid;
	add_query_field_str(query, "uid", std_fields, &uid, vals);
	
	int status;
	add_query_field_int(query, "status", std_fields, &status, vals, false);
	
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

void LogStorage::add_query_field_str(string &query, const string &name, map<string, string> &fields, string *val, vector<void *> &vals)
{
	auto it = fields.find(name);
	if(it==fields.end())
	{
		query += ", NULL";
		return;
	}
	
	*val = it->second;
	query += ", %s";
	vals.push_back(val);
}

void LogStorage::add_query_field_int(string &query, const string &name, map<string, string> &fields, int *val, vector<void *> &vals, bool need_pack)
{
	auto it = fields.find(name);
	if(it==fields.end())
	{
		query += ", NULL";
		return;
	}
	
	if(need_pack)
		*val = pack(it->second);
	else
		*val = stoi(it->second);
	
	query += ", %i";
	vals.push_back(val);
}

unsigned int LogStorage::pack(const string &str)
{
	auto it = pack_str_id.find(str);
	
	if(it!=pack_str_id.end())
		return it->second;
	
	DB db;
	db.QueryPrintf("REPLACE INTO t_elog_pack(elog_pack_string) VALUES(%s)", &str);
	
	unsigned int id = db.InsertID();
	pack_str_id[str] = id;
	
	return id;
}

