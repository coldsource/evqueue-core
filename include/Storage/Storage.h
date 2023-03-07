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

#ifndef _STORAGE_H_
#define _STORAGE_H_

#include <string>

#include <nlohmann/json.hpp>

#define STORAGE_VAR_MAXLEN     0xFFFFFF
#define STORAGE_NAME_MAXLEN    64
#define STORAGE_PATH_MAXLEN    255
#define STORAGE_DESC_MAXLEN    0xFFFF

class XMLQuery;
class QueryResponse;
class User;

namespace Storage
{

class Storage
{
	static void check_path(const std::string &path);
	static void check_name(const std::string &name);
	static void check_value(const std::string &value, const std::string type, const std::string structure);
	static void check_value_type(const nlohmann::json &j, const std::string &type);
	
	public:
		static unsigned int Set(unsigned int id, const std::string &path, const std::string &name, const std::string &type, const std::string &structure, const std::string &value);
		static unsigned int Unset(unsigned int id, const std::string &path, const std::string &name);
		static void Get(unsigned int id, const std::string &path, const std::string &name, QueryResponse *response);
		static void List(const std::string &path, bool recursive, QueryResponse *response);
		
		static bool HandleQuery(const User &user, XMLQuery *query, QueryResponse *response);
};

}

#endif
