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

#ifndef __DB_H__
#define __DB_H__

#include <mysql/mysql.h>

class DB
{
	MYSQL *mysql;
	MYSQL_RES *res;
	MYSQL_ROW row;
	
	bool is_copy;
	
public:
	DB(const char *host,const char *user,const char *password,const char *dbname);
	DB(const DB *db);
	DB(void);
	~DB(void);
	
	DB *Clone(void);
	
	void Ping(void);
	void Query(const char *query);
	void QueryPrintf(const char *query,...);
	void EscapeString(const char *string, char *escaped_string);
	int InsertID(void);
	
	bool FetchRow(void);
	void Seek(int offset);
	void Free(void);
	int NumRows(void);
	int AffectedRows(void);
	
	char *GetField(int n);
	int GetFieldInt(int n);
	double GetFieldDouble(int n);
};

#endif
