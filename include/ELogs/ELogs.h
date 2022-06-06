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

#ifndef _ELOGS_H_
#define _ELOGS_H_

class XMLQuery;
class QueryResponse;
class User;

#include <vector>
#include <string>
#include <map>

namespace ELogs
{

class Fields;

class ELogs
{
	static std::string get_filter(const std::map<std::string, std::string> &filters, const std::string &name, const std::string &default_val);
	static int get_filter(const std::map<std::string, std::string> &filters, const std::string &name,int default_val);
	static void add_auto_filters(const std::map<std::string, std::string> filters, const Fields &fields, const std::string &prefix, std::string &query_where, std::vector<const void *> &values, int *val_int, std::string *val_str);
	
	public:
		static std::vector<std::map<std::string, std::string>> QueryLogs(std::map<std::string, std::string> filters, unsigned int limit, unsigned int offset = 0);
		
		static bool HandleQuery(const User &user, XMLQuery *query, QueryResponse *response);
};

}

#endif
