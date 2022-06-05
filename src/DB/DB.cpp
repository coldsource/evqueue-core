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

#include <DB/DB.h>
#include <DB/DBConfig.h>
#include <Exception/Exception.h>
#include <Logger/Logger.h>
#include <Configuration/ConfigurationEvQueue.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>

#include <vector>
#include <string>
#include <regex>

using namespace std;

static auto initdb =  DBConfig::GetInstance()->RegisterConfigInit([](DBConfig *dbconf) {
	Configuration *config = Configuration::GetInstance();
	string host = config->Get("mysql.host");
	string user = config->Get("mysql.user");
	string password = config->Get("mysql.password");
	string database = config->Get("mysql.database");
	dbconf->RegisterConfig("evqueue", host, user, password, database);
});

DB::DB(DB *db)
{
	host = db->host;
	user = db->user;
	password = db->password;
	database = db->database;
	
	// We share the same MySQL handle but is_connected is split. We must be connected before cloning or the connection will be made twice
	db->connect();
	
	mysql = db->mysql;
	res = 0;
	transaction_started = 0;
	is_connected = db->is_connected;
	is_copy = true;
}

DB::DB(const string &name, bool nodbselect)
{
	// Initialisation de mysql
	mysql = mysql_init(0);
	mysql_options(mysql, MYSQL_SET_CHARSET_NAME, "UTF8");
	
	res=0;
	transaction_started = 0;
	is_connected = false;
	is_copy = false;
	
	// Read database configuration
	DBConfig::GetInstance()->GetConfig(name, host, user, password, database);
	if(nodbselect)
		database = "";
}

DB::~DB(void)
{
	if(res)
		mysql_free_result(res);

	if(mysql && !is_copy)
		mysql_close(mysql);
}

DB *DB::Clone(void)
{
	return new DB(this);
}

void DB::InitLibrary(void)
{
	mysql_library_init(0,0,0);
}

void DB::FreeLibrary(void)
{
	mysql_library_end();
}

void DB::StartThread(void)
{
	mysql_thread_init();
}

void DB::StopThread(void)
{
	mysql_thread_end();
}

void DB::Ping(void)
{
	connect();
	
	if(mysql_ping(mysql)!=0)
		throw Exception("DB",mysql_error(mysql),"SQL_ERROR",mysql_errno(mysql));
}

void DB::Wait(void)
{
	while(true)
	{
		try
		{
			Ping();
		}
		catch(Exception &e)
		{
			if(e.codeno==2002 || e.codeno==2013)
			{
				Logger::Log(LOG_WARNING, "Database not yet ready, retrying...");
				sleep(1);
				continue;
			}
			
			throw e;
		}
		
		break;
	}
}

void DB::Query(const string &query)
{
	for(int i=0;i<2;i++)
	{
		connect();
		
		if(res)
		{
			mysql_free_result(res);
			res=0;
		}

		Logger::Log(LOG_DEBUG, "Executing query : " + query);
		
		if(mysql_query(mysql,query.c_str())!=0)
		{
			string error = mysql_error(mysql);
			int code = mysql_errno(mysql);
			
			if(!transaction_started && code==2006)
			{
				is_connected = false;
				continue; // Auto retry once if we just have been disconnected
			}
			
			if(auto_rollback && transaction_started)
				RollbackTransaction();
			
			throw Exception("DB",error,"SQL_ERROR",code);
		}

		res=mysql_store_result(mysql);
		if(res==0)
		{
			if(mysql_field_count(mysql)!=0)
				throw Exception("DB",mysql_error(mysql),"SQL_ERROR",mysql_errno(mysql));
		}
		
		return;
	}
	
	throw Exception("DB","Reconnected to database, but still getting gone away error");
}

void DB::QueryPrintf(const string &query,const vector<const void *> &args)
{
	regex prct_regex("%(c|s|i|l)");
	
	auto words_begin = sregex_iterator(query.begin(), query.end(), prct_regex);
	auto words_end = sregex_iterator();
	
	int last_pos = 0;
	int match_i = 0;
	string escaped_query;
	for (sregex_iterator i = words_begin; i != words_end; ++i)
	{
		smatch m = *i;
		escaped_query += query.substr(last_pos, m.position()-last_pos) + get_query_value(m.str()[1], match_i, args);
		last_pos = m.position()+m.length();
		
		match_i++;
	}
	
	escaped_query += query.substr(last_pos);
	
	Query(escaped_query);
}

string DB::get_query_value(char type, int idx, const std::vector<const void *> &args)
{
	if(args[idx]==0)
		return "NULL";
	
	switch(type)
	{
		case 'c':
			return "`"+(*((string *)args[idx]))+"`";
		case 's':
			return "'" + EscapeString(*((const string *)args[idx])) + "'";
		case 'i':
			return to_string(*(const int *)args[idx]);
		case 'l':
			return to_string(*(const long long *)args[idx]);
	}
	
	return "";
}

string DB::EscapeString(const string &str)
{
	char buf[2*str.size()+1];
	long unsigned int size = mysql_real_escape_string(mysql, buf, str.c_str(), str.size());
	return string(buf, size);
}

int DB::InsertID(void)
{
	return mysql_insert_id(mysql);
}

long long DB::InsertIDLong(void)
{
	return mysql_insert_id(mysql);
}

void DB::BulkStart(int bulk_id, const std::string &table, const std::string &columns, int ncolumns)
{
	bulk_queries[bulk_id] = {table, columns, ncolumns};
}

void DB::BulkDataNULL(int bulk_id)
{
	st_bulk_value v;
	v.type = st_bulk_value::en_type::N;
	bulk_queries[bulk_id].values.push_back(v);
}

void DB::BulkDataInt(int bulk_id, int i)
{
	st_bulk_value v;
	v.type = st_bulk_value::en_type::INT;
	v.val_int = i;
	bulk_queries[bulk_id].values.push_back(v);
}

void DB::BulkDataLong(int bulk_id, long long ll)
{
	st_bulk_value v;
	v.type = st_bulk_value::en_type::LONG;
	v.val_ll = ll;
	bulk_queries[bulk_id].values.push_back(v);
}

void DB::BulkDataString(int bulk_id, const std::string &s)
{
	st_bulk_value v;
	v.type = st_bulk_value::en_type::STRING;
	v.val_str = s;
	bulk_queries[bulk_id].values.push_back(v);
}

void DB::BulkExec(int bulk_id)
{
	const st_bulk_query &bulk_query = bulk_queries[bulk_id];
	const st_bulk_value *values = bulk_query.values.data();
	
	string query = "INSERT INTO "+bulk_query.table+"("+bulk_query.columns+") VALUES";
	int nrows = bulk_query.values.size()/bulk_query.ncolumns;
	int n = 0;
	for(int i=0;i<nrows;i++)
	{
		if(i>0)
			query += ",";
		
		query += "(";
		for(int j=0;j<bulk_query.ncolumns;j++)
		{
			if(j>0)
				query += ",";
			
			if(values[n].type==st_bulk_value::en_type::N)
				query += "NULL";
			else if(values[n].type==st_bulk_value::en_type::INT)
				query += to_string(values[n].val_int);
			else if(values[n].type==st_bulk_value::en_type::LONG)
				query += to_string(values[n].val_ll);
			else if(values[n].type==st_bulk_value::en_type::STRING)
				query += "'"+EscapeString(values[n].val_str)+"'";
			
			n++;
		}
		query += ")";
	}
	
	bulk_queries.erase(bulk_id);
	if(nrows==0)
		return;
	
	Query(query.c_str());
}

bool DB::FetchRow(void)
{
	if(res==0)
		return false;

	row=mysql_fetch_row(res);
	row_field_length=mysql_fetch_lengths(res);

	if(row)
		return true;

	return false;
}

void DB::Seek(int offset)
{
	if(res)
		mysql_data_seek(res,offset);
}

void DB::Free(void)
{
	if(res)
	{
		mysql_free_result(res);
		res=0;
	}
}

int DB::NumRows(void)
{
	if(res)
		return mysql_num_rows(res);
	return -1;
}

int DB::AffectedRows(void)
{
	return mysql_affected_rows(mysql);
}

void DB::StartTransaction()
{
	Query("START TRANSACTION");
	transaction_started = true;
}

void DB::CommitTransaction()
{
	Query("COMMIT");
	transaction_started = false;
}

void DB::RollbackTransaction()
{
	Query("ROLLBACK");
	transaction_started = false;
}

bool DB::GetFieldIsNULL(int n)
{
	if(res==0 || row==0)
		return true;

	return row[n]==0;
}

string DB::GetField(int n)
{
	if(res==0 || row==0)
		return "";

	if(!row[n])
		return "";
	
	return string(row[n], row_field_length[n]);
}

int DB::GetFieldInt(int n)
{
	string v = GetField(n);
	if(v=="")
		return 0;
	
	int ival;
	try
	{
		ival = stoi(v);
	}
	catch(...)
	{
		throw Exception("DB",v+" is not an integer value","SQL_ERROR",mysql_errno(mysql));
	}
	return ival;
}

long long DB::GetFieldLong(int n)
{
	string v = GetField(n);
	if(v=="")
		return 0;
	
	long long ival;
	try
	{
		ival = stoll(v);
	}
	catch(...)
	{
		throw Exception("DB",v+" is not an integer value","SQL_ERROR",mysql_errno(mysql));
	}
	return ival;
}

double DB::GetFieldDouble(int n)
{
	string v = GetField(n);
	if(v=="")
		return 0;
	
	return stof(v);
}

unsigned long DB::GetFieldLength(int n)
{
	if(res==0 || row==0)
		return 0;

	return row_field_length[n];
}

void DB::Disconnect()
{
	if(is_connected && !is_copy)
	{
		mysql_close(mysql);
		mysql = 0;
		is_connected = false;
	}
}

// Function similar to MySQL TO_DAYS (number of days since year 0)
int DB::TO_DAYS(const string &t)
{
	struct tm end_t = { 0 };
	char *ptr = strptime(t.c_str(), "%Y-%m-%d %H:%M:%S" , &end_t);
	if(!ptr)
		return -1;
	
	time_t end = mktime(&end_t);
	
	struct tm start_t = {0,0,0,1,0,-1900}; // 1st Jan 0
	time_t start = mktime(&start_t);
	
	return (int)(difftime(end, start) / 86400);
}

void DB::connect(void)
{
	if(is_connected)
		return; // Nothing to do
	
	const char *dbptr = 0;
	if(database!="")
		dbptr = database.c_str();
	
	if(!mysql_real_connect(mysql, host.c_str(), user.c_str(), password.c_str(), dbptr, 0, 0, 0))
		throw Exception("DB",mysql_error(mysql),"SQL_ERROR",mysql_errno(mysql));
	
	is_connected = true;
}
