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

#ifndef _ALERT_H_
#define _ALERT_H_

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

class XMLQuery;
class QueryResponse;
class User;
class DB;

namespace ELogs
{

class Alert
{
	unsigned int id;
	std::string name;
	std::string description;
	unsigned int occurrences;
	unsigned int period;
	std::string filters;
	bool is_groupped = false;
	int active;
	
	std::vector<unsigned int> notifications;
	
	nlohmann::json json_filters;
	
	public:
		Alert();
		Alert(DB *db,unsigned int alert_id);
		
		unsigned int GetID() const { return id; }
		const std::string &GetName() const { return name; }
		const std::string &GetDescription() const { return description; }
		unsigned int GetOccurrences() const { return occurrences; }
		unsigned int GetPeriod() const { return period; }
		const std::string &GetFilters() const { return filters; }
		const nlohmann::json &GetJsonFilters() const { return json_filters; }
		std::vector<unsigned int> GetNotifications() const { return notifications; }
		bool GetIsGroupped() const { return is_groupped; }
		bool GetIsActive() const { return active!=0; }
		
		
		static bool CheckName(const std::string &alert_name);
		static void Get(unsigned int id, QueryResponse *response);
		static unsigned int Create(const std::string &name, const std::string &description, unsigned int occurrences, unsigned int period, const std::string &filters, const std::string &notifications, bool active);
		static void Edit(unsigned int id, const std::string &name, const std::string &description, unsigned int occurrences, unsigned int period, const std::string &filters, const std::string &notifications, bool active);
		static void Delete(unsigned int id);
		static void SetIsActive(unsigned int id, bool active);
		
		static bool HandleQuery(const User &user, XMLQuery *query, QueryResponse *response);
		
		static bool test();
	
	private:
		static void create_edit_check(const std::string &name, unsigned int occurencies, unsigned int period, const std::string &filters, const std::string &notifications);
};

}

#endif
