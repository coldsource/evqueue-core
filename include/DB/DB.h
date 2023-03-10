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
#include <map>

class DB
{
	struct st_bulk_value
	{
		enum en_type
		{
			N,
			INT,
			LONG,
			STRING
		};
		
		en_type type;
		int val_int;
		long long val_ll;
		std::string val_str;
	};
	
	struct st_bulk_query
	{
		std::string table;
		std::string columns;
		int ncolumns;
		std::vector<st_bulk_value> values;
	};
	
	std::map<int, st_bulk_query> bulk_queries;
	
	MYSQL *mysql;
	MYSQL_RES *res;
	MYSQL_ROW row;
	unsigned long *row_field_length;
	
	bool transaction_started;
	bool auto_rollback = true;
	
	bool is_connected;
	bool is_copy;
	
	std::string host;
	std::string user;
	std::string password;
	std::string database;
	
public:
	DB(DB *db);
	DB(const std::string &name = "evqueue", bool nodbselect = false);
	~DB(void);
	
	DB *Clone(void);
	
	static void InitLibrary(void);
	static void FreeLibrary(void);
	
	static void StartThread(void);
	static void StopThread(void);
	
	std::string GetDatabase() { return database; }
	
	void Ping(void);
	void Wait(void);
	
	void Query(const std::string &query);
	void QueryPrintf(const std::string &query,const std::vector<const void *> &args = {});
	std::string EscapeString(const std::string &str);
	int InsertID(void);
	long long InsertIDLong(void);
	
	void BulkStart(int bulk_id, const std::string &table, const std::string &columns, int ncolumns);
	void BulkDataNULL(int bulk_id);
	void BulkDataInt(int bulk_id, int i);
	void BulkDataLong(int bulk_id, long long ll);
	void BulkDataString(int bulk_id, const std::string &s);
	void BulkExec(int bulk_id);
	
	bool FetchRow(void);
	void Seek(int offset);
	void Free(void);
	int NumRows(void);
	int AffectedRows(void);
	
	void StartTransaction();
	void CommitTransaction();
	void RollbackTransaction();
	void SetAutoRollback(bool v) { auto_rollback = v; }
	
	bool GetFieldIsNULL(int n) const;
	std::string GetField(int n) const;
	int GetFieldInt(int n) const;
	long long GetFieldLong(int n) const;
	double GetFieldDouble(int n) const;
	unsigned long GetFieldLength(int n) const;
	
	void Disconnect();
	
	static int TO_DAYS(const std::string &t);
	
private:
	void connect();
	std::string get_query_value(char type, int idx, const std::vector<const void *> &args); 
};

#endif
