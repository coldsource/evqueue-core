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

#ifndef _CHANNEL_H_
#define _CHANNEL_H_

#include <string>
#include <regex>
#include <map>

#include <nlohmann/json.hpp>

class XMLQuery;
class QueryResponse;
class User;
class DB;

class Channel
{
	unsigned int elog_channel_id;
	std::string elog_channel_name;
	std::string elog_channel_config;
	
	nlohmann::json json_config;
	
	std::regex log_regex;
	
	int crit;
	int crit_idx;
	int machine_idx;
	int domain_idx;
	int ip_idx;
	int uid_idx;
	int status_idx;
	std::map<std::string, int> custom_fields_idx;
	
	public:
		Channel();
		Channel(DB *db,unsigned int elog_channel_id);
		Channel(unsigned int id, const std::string &name, const std::string &config);
		
		unsigned int GetID() const { return elog_channel_id; }
		const std::string &GetName() const { return elog_channel_name; }
		const std::string &GetConfig() const { return elog_channel_config; }
		
		void ParseLog(const std::string log_str, std::map<std::string, std::string> &std_fields, std::map<std::string, std::string> &custom_fields) const;
		
		static bool CheckChannelName(const std::string &user_name);
		static void Get(unsigned int id, QueryResponse *response);
		static unsigned int Create(const std::string &name, const std::string &config);
		static void Edit(unsigned int id, const std::string &name, const std::string &config);
		static void Delete(unsigned int id);
		
		static bool HandleQuery(const User &user, XMLQuery *query, QueryResponse *response);
	
	private:
		void init(unsigned int id, const std::string &name, const std::string &config);
		void get_log_part(const std::smatch &matches, const std::string &name, int idx, std::map<std::string, std::string> &val) const;
		int get_log_idx(const nlohmann::json &j, const std::string &name);
		static bool check_int_field(const nlohmann::json &j, const std::string &name);
		static int str_to_crit(const std::string &str);
		static std::string crit_to_str(int i);
		static void create_edit_check(const std::string &name, const std::string &config);
};

#endif
