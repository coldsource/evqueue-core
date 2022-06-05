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

#ifndef __DBCONFIG_H__
#define __DBCONFIG_H__

#include <string>
#include <vector>
#include <map>

class DBConfig
{
	public:
		typedef void (*t_dbconfig_init) (DBConfig *);
		
	private:
		struct t_config
		{
			std::string host;
			std::string user;
			std::string password;
			std::string database;
		};
		
		std::vector<t_dbconfig_init> configs_init;
		std::map<std::string, t_config> configs;
		std::map<std::string, std::map<std::string, std::string>> tables;
		std::map<std::string, std::map<std::string, std::string>> tables_init;
		
		static DBConfig *instance;
	
	public:
		static DBConfig *GetInstance();
		
		bool RegisterConfigInit(t_dbconfig_init init);
		void Init();
		
		void Check(bool wait);
		
		void RegisterConfig(const std::string &name, const std::string &host, const std::string &user, const std::string &password, const std::string &database);
		void GetConfig(const std::string &name, std::string &host, std::string &user, std::string &password, std::string &database);
		
		void InitDatabases();
		
		bool RegisterTables(const std::string &name, std::map<std::string, std::string> &tables_def);
		bool RegisterTablesInit(const std::string &name, std::map<std::string, std::string> &tables_query);
		void InitTables();
};

#endif
