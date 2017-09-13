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

#include <DB.h>
#include <Exception.h>
#include <Logger.h>
#include <Configuration.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include <vector>
#include <string>

using namespace std;

DB::DB(DB *db)
{
	// We share the same MySQL handle but is_connected is split. We must be connected before cloning or the connection will be made twice
	db->connect();
	
	mysql = db->mysql;
	res = 0;
	transaction_started = 0;
	is_connected = db->is_connected;
	is_copy = true;
}

DB::DB(void)
{
	// Initialisation de mysql
	mysql = mysql_init(0);
	mysql_options(mysql, MYSQL_SET_CHARSET_NAME, "UTF8");
	
	res=0;
	transaction_started = 0;
	is_connected = false;
	is_copy = false;
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

void DB::Ping(void)
{
	connect();
	
	if(mysql_ping(mysql)!=0)
		throw Exception("DB",mysql_error(mysql));
}

void DB::Query(const char *query)
{
	connect();
	
	if(res)
	{
		mysql_free_result(res);
		res=0;
	}

	Logger::Log(LOG_DEBUG, "Executing query : %s",query);
	
	if(mysql_query(mysql,query)!=0)
	{
		if(transaction_started)
			RollbackTransaction();
		
		throw Exception("DB",mysql_error(mysql));
	}

	res=mysql_store_result(mysql);
	if(res==0)
	{
		if(mysql_field_count(mysql)!=0)
			throw Exception("DB",mysql_error(mysql));
	}
}

void DB::QueryPrintfC(const char *query,...)
{
	int len,escaped_len;
	const char *arg_str;
	const int *arg_int;
	va_list ap;

	va_start(ap,query);

	len = strlen(query);
	escaped_len = 0;
	for(int i=0;i<len;i++)
	{
		if(query[i]=='%')
		{
			switch(query[i+1])
			{
				case 's':
					arg_str = va_arg(ap,const char *);
					if(arg_str)
						escaped_len += 2+2*strlen(arg_str); // 2 Quotes + Escaped string
					else
						escaped_len += 4; // NULL
					i++;
					break;
				
				case 'i':
					arg_int = va_arg(ap,const int *);
					if(arg_int)
						escaped_len += 16; // Integer
					else
						escaped_len += 4; // NULL
					i++;
					break;

				default:
					escaped_len++;
					break;
			}
		}
		else
			escaped_len++;
	}

	va_end(ap);


	va_start(ap,query);

	char *escaped_query = new char[escaped_len+1];
	int j = 0;
	for(int i=0;i<len;i++)
	{
		if(query[i]=='%')
		{
			switch(query[i+1])
			{
				case 's':
					arg_str = va_arg(ap,const char *);
					if(arg_str)
					{
						escaped_query[j++] = '\'';
						j += mysql_real_escape_string(mysql, escaped_query+j, arg_str, strlen(arg_str));
						escaped_query[j++] = '\'';
					}
					else
					{
						strcpy(escaped_query+j,"NULL");
						j += 4;
					}
					i++;
					break;
				
				case 'i':
					arg_int = va_arg(ap,const int *);
					if(arg_int)
						j += sprintf(escaped_query+j,"%d",*arg_int);
					else
					{
						strcpy(escaped_query+j,"NULL");
						j += 4;
					}
					i++;
					break;

				default:
					escaped_query[j++] = query[i];
					break;
			}
		}
		else
			escaped_query[j++] = query[i];
	}

	va_end(ap);

	escaped_query[j] = '\0';

	try
	{
		Query(escaped_query);
	}
	catch(Exception &e)
	{
		delete[] escaped_query;
		throw e;
	}
	
	delete[] escaped_query;
}

void DB::QueryPrintf(const string &query,...)
{
	int len,escaped_len;
	const string *arg_str;
	const int *arg_int;
	va_list ap;

	va_start(ap,query);

	len = query.length();
	escaped_len = 0;
	for(int i=0;i<len;i++)
	{
		if(query[i]=='%')
		{
			switch(query[i+1])
			{
				case 's':
					arg_str = va_arg(ap,const string *);
					if(arg_str)
						escaped_len += 2+2*arg_str->length(); // 2 Quotes + Escaped string
					else
						escaped_len += 4; // NULL
					i++;
					break;
				
				case 'i':
					arg_int = va_arg(ap,const int *);
					if(arg_int)
						escaped_len += 16; // Integer
					else
						escaped_len += 4; // NULL
					i++;
					break;

				default:
					escaped_len++;
					break;
			}
		}
		else
			escaped_len++;
	}

	va_end(ap);


	va_start(ap,query);

	char *escaped_query = new char[escaped_len+1];
	int j = 0;
	for(int i=0;i<len;i++)
	{
		if(query[i]=='%')
		{
			switch(query[i+1])
			{
				case 's':
					arg_str = va_arg(ap,const string *);
					if(arg_str)
					{
						escaped_query[j++] = '\'';
						j += mysql_real_escape_string(mysql, escaped_query+j, arg_str->c_str(), arg_str->length());
						escaped_query[j++] = '\'';
					}
					else
					{
						strcpy(escaped_query+j,"NULL");
						j += 4;
					}
					i++;
					break;
				
				case 'i':
					arg_int = va_arg(ap,const int *);
					if(arg_int)
						j += sprintf(escaped_query+j,"%d",*arg_int);
					else
					{
						strcpy(escaped_query+j,"NULL");
						j += 4;
					}
					i++;
					break;

				default:
					escaped_query[j++] = query[i];
					break;
			}
		}
		else
			escaped_query[j++] = query[i];
	}

	va_end(ap);

	escaped_query[j] = '\0';

	try
	{
		Query(escaped_query);
	}
	catch(Exception &e)
	{
		delete[] escaped_query;
		throw e;
	}
	
	delete[] escaped_query;
}

void DB::QueryVsPrintf(const string &query,const vector<void *> &args)
{
	int len,escaped_len;
	const string *arg_str;
	const int *arg_int;
	
	int cur = 0;

	len = query.length();
	escaped_len = 0;
	for(int i=0;i<len;i++)
	{
		if(query[i]=='%')
		{
			switch(query[i+1])
			{
				case 's':
					arg_str = (string *)args.at(cur++);
					if(arg_str)
						escaped_len += 2+2*arg_str->length(); // 2 Quotes + Escaped string
					else
						escaped_len += 4; // NULL
					i++;
					break;
				
				case 'i':
					arg_int = (int *)args.at(cur++);
					if(arg_int)
						escaped_len += 16; // Integer
					else
						escaped_len += 4; // NULL
					i++;
					break;

				default:
					escaped_len++;
					break;
			}
		}
		else
			escaped_len++;
	}

	cur = 0;

	char *escaped_query = new char[escaped_len+1];
	int j = 0;
	for(int i=0;i<len;i++)
	{
		if(query[i]=='%')
		{
			switch(query[i+1])
			{
				case 's':
					arg_str = (string *)args.at(cur++);
					if(arg_str)
					{
						escaped_query[j++] = '\'';
						j += mysql_real_escape_string(mysql, escaped_query+j, arg_str->c_str(), arg_str->length());
						escaped_query[j++] = '\'';
					}
					else
					{
						strcpy(escaped_query+j,"NULL");
						j += 4;
					}
					i++;
					break;
				
				case 'i':
					arg_int = (int *)args.at(cur++);
					if(arg_int)
						j += sprintf(escaped_query+j,"%d",*arg_int);
					else
					{
						strcpy(escaped_query+j,"NULL");
						j += 4;
					}
					i++;
					break;

				default:
					escaped_query[j++] = query[i];
					break;
			}
		}
		else
			escaped_query[j++] = query[i];
	}

	escaped_query[j] = '\0';

	try
	{
		Query(escaped_query);
	}
	catch(Exception &e)
	{
		delete[] escaped_query;
		throw e;
	}
	
	delete[] escaped_query;
}

void DB::EscapeString(const char *string, char *escaped_string)
{
	mysql_real_escape_string(mysql, escaped_string, string, strlen(string));
}

int DB::InsertID(void)
{
	return mysql_insert_id(mysql);
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
	
	return string(row[n]);
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
		throw Exception("DB",v+" is not an integer value");
	}
	return ival
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

void DB::connect(void)
{
	if(is_connected)
		return; // Nothing to do
	
	Configuration *config = Configuration::GetInstance();
	
	if(!mysql_real_connect(mysql,config->Get("mysql.host").c_str(),config->Get("mysql.user").c_str(),config->Get("mysql.password").c_str(),config->Get("mysql.database").c_str(),0,0,0))
		throw Exception("DB",mysql_error(mysql));
	
	is_connected = true;
}
