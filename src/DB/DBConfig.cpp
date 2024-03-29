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

#include <DB/DBConfig.h>
#include <DB/DB.h>
#include <Exception/Exception.h>
#include <Logger/Logger.h>

using namespace std;

DBConfig *DBConfig::instance = 0;

DBConfig *DBConfig::GetInstance()
{
	if(instance == 0)
		instance = new DBConfig();
	
	return instance;
}

bool DBConfig::RegisterConfigInit(t_dbconfig_init init)
{
	configs_init.push_back(init);
	return true;
}

void DBConfig::Init()
{
	for(int i=0;i<configs_init.size(); i++)
		configs_init[i](this);
}

void DBConfig::Check(bool wait)
{
	for(auto it = configs.begin(); it!=configs.end(); ++it)
	{
		DB db(it->first, true);
		if(wait)
			db.Wait();
		else
			db.Ping();
	}
}

void DBConfig::RegisterConfig(const string &name, const string &host, const string &user, const string &password, const string &database)
{
	if(configs.find(name)!=configs.end())
		throw Exception("DBConfig", "DB configuration is already registered : "+name);
	
	configs[name] = {host, user, password, database};
}

void DBConfig::GetConfig(const string &name, string &host, string &user, string &password, string &database)
{
	if(configs.find(name)==configs.end())
		throw Exception("DBConfig", "Unknown DB config : "+name);
	
	auto c = configs[name];
	host = c.host;
	user = c.user;
	password = c.password;
	database = c.database;
}

void DBConfig::InitDatabases()
{
	for(auto it = configs.begin(); it!=configs.end(); ++it)
	{
		auto dbname = it->second.database;
		
		DB db(it->first, true);
		db.QueryPrintf("SELECT COUNT(*) FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME=%s", {&dbname});
		if(db.FetchRow() && db.GetFieldInt(0)==0)
		{
			Logger::Log(LOG_NOTICE,it->first + " database (" + it->second.database + ") does not exists, creating it...");
			
			try
			{
				db.QueryPrintf("CREATE DATABASE %c", {&dbname});
			}
			catch(Exception &e)
			{
				throw Exception("DBConfig", "Error creating database " + it->second.database + " ("+e.error+")");
			}
		}
	}
}

bool DBConfig::RegisterInit(t_db_handler_init init)
{
	this->init.push_back(init);
	return true;
}

bool DBConfig::RegisterTables(const string &name, map<string, string> &tables_def)
{
	if(tables.find(name)!=tables.end())
	{
		// Merge new tables in existing scheme
		for(auto it = tables_def.begin(); it!=tables_def.end(); ++it)
			tables[name][it->first] = it->second;
	}
	else
		tables[name] = tables_def;
	
	return true;
}

bool DBConfig::RegisterTablesInit(const string &name, map<string, string> &tables_query)
{
	if(tables_init.find(name)!=tables_init.end())
	{
		
		// Merge new queries in existing scheme
		for(auto it = tables_query.begin(); it!=tables_query.end(); ++it)
			tables_init[name][it->first] = it->second;
	}
	else
		tables_init[name] = tables_query;
	
	return true;
}

void DBConfig::InitTables()
{
	// Call all init handlers
	for(int i=0;i<init.size();i++)
		(init[i])(this);
	
	// Then init tables
	for(auto it_db = tables.begin(); it_db!=tables.end(); ++it_db)
	{
		auto tables_def = it_db->second;
		
		DB db(it_db->first);
		for(auto it_table=tables_def.begin();it_table!=tables_def.end();++it_table)
		{
			string database = db.GetDatabase();
			db.QueryPrintf(
				"SELECT table_comment FROM INFORMATION_SCHEMA.TABLES WHERE table_schema=%s AND table_name=%s", {
				&database,
				&it_table->first
			});
			
			if(!db.FetchRow())
			{
				Logger::Log(LOG_NOTICE,it_db->first + " table " + it_table->first + " does not exists, creating it...");
				
				db.QueryPrintf(it_table->second);
				
				if(tables_init.find(it_db->first)!=tables_init.end())
				{
					auto init_q = tables_init[it_db->first];
					if(init_q.find(it_table->first)!=init_q.end())
						db.QueryPrintf(init_q[it_table->first]);
				}
			}
		}
	}
}
