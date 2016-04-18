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
#include <Configuration.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

DB::DB(const char *host,const char *user,const char *password,const char *dbname)
{
	// Initialisation de mysql
	mysql = mysql_init(0);

	// Connection à la base
	if(!mysql_real_connect(mysql,host,user,password,dbname,0,0,0))
		throw Exception("DB",mysql_error(mysql));

	res=0;
	is_copy = false;
	
	Query("SET NAMES 'UTF8'");
}

DB::DB(const DB *db)
{
	mysql = db->mysql;
	res = 0;
	is_copy = true;
}

DB::DB(void)
{
	Configuration *config = Configuration::GetInstance();

	// Initialisation de mysql
	mysql = mysql_init(0);
	mysql_options(mysql, MYSQL_SET_CHARSET_NAME, "UTF8");

	// Connection à la base
	if(!mysql_real_connect(mysql,config->Get("mysql.host").c_str(),config->Get("mysql.user").c_str(),config->Get("mysql.password").c_str(),config->Get("mysql.database").c_str(),0,0,0))
		throw Exception("DB",mysql_error(mysql));

	res=0;
	is_copy = false;
}

DB::~DB(void)
{
	if(res)
		mysql_free_result(res);

	if(!is_copy)
		mysql_close(mysql);
}

DB *DB::Clone(void)
{
	return new DB(this);
}

void DB::Ping(void)
{
	if(mysql_ping(mysql)!=0)
		throw Exception("DB",mysql_error(mysql));
}

void DB::Query(const char *query)
{
	if(res)
	{
		mysql_free_result(res);
		res=0;
	}

	if(mysql_query(mysql,query)!=0)
		throw Exception("DB",mysql_error(mysql));

	res=mysql_store_result(mysql);
	if(res==0)
	{
		if(mysql_field_count(mysql)!=0)
			throw Exception("DB",mysql_error(mysql));
	}
}

void DB::QueryPrintf(const char *query,...)
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

	Query(escaped_query);

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

char *DB::GetField(int n)
{
	if(res==0 || row==0)
		return 0;

	return row[n];
}

int DB::GetFieldInt(int n)
{
	const char *v = GetField(n);
	if(!v)
		return 0;
	
	return atoi(v);
}

double DB::GetFieldDouble(int n)
{
	const char *v = GetField(n);
	if(!v)
		return 0;
	
	return atof(v);
}

unsigned long DB::GetFieldLength(int n)
{
	if(res==0 || row==0)
		return 0;

	return row_field_length[n];
}
