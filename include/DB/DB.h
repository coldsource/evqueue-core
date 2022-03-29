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

#if defined (MYSQL_PATH_MYSQL) || defined (MYSQL_PATH_MARIADB)

#ifdef MYSQL_PATH_MYSQL
#include <mysql/mysql.h>
#endif

#ifdef MYSQL_PATH_MARIADB
#include <mariadb/mysql.h>
#endif

#else
#include <mysql/mysql.h>
#endif

#include <string>
#include <vector>

class DB
{
	MYSQL *mysql;
	MYSQL_RES *res;
	MYSQL_ROW row;
	unsigned long *row_field_length;
	
	bool transaction_started;
	
	bool is_connected;
	bool is_copy;
	
	std::string host;
	std::string user;
	std::string password;
	std::string database;
	
public:
	DB(DB *db);
	DB(const std::string &name = "");
	~DB(void);
	
	DB *Clone(void);
	
	static void InitLibrary(void);
	static void FreeLibrary(void);
	
	static void StartThread(void);
	static void StopThread(void);
	
	void Ping(void);
	void Query(const char *query);
	void QueryPrintfC(const char *query,...);
	void QueryPrintf(const std::string &query,...);
	void QueryVsPrintf(const std::string &query,const std::vector<void *> &args);
	void EscapeString(const char *string, char *escaped_string);
	int InsertID(void);
	long long InsertIDLong(void);
	
	bool FetchRow(void);
	void Seek(int offset);
	void Free(void);
	int NumRows(void);
	int AffectedRows(void);
	
	void StartTransaction();
	void CommitTransaction();
	void RollbackTransaction();
	
	bool GetFieldIsNULL(int n);
	std::string GetField(int n);
	int GetFieldInt(int n);
	long long GetFieldLong(int n);
	double GetFieldDouble(int n);
	unsigned long GetFieldLength(int n);
	
	void Disconnect();
	
	static int TO_DAYS(const std::string &t);
	
private:
	void connect();
};

#endif
