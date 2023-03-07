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

#ifndef _LAUNCHER_H_
#define _LAUNCHER_H_

#include <string>

#include <nlohmann/json.hpp>

class XMLQuery;
class QueryResponse;
class User;

namespace Storage
{

class Launcher
{
	static void create_edit_check(
		const std::string &name,
		const std::string group,
		const std::string &description,
		unsigned int workflow_id,
		const std::string &user,
		const std::string &host,
		const std::string &parameters);
	
	public:
		static void Get(unsigned int id, QueryResponse *response);
		static unsigned int Create(
			const std::string &name,
			const std::string group,
			const std::string &description,
			unsigned int workflow_id,
			const std::string &user,
			const std::string &host,
			const std::string &parameters);
		
		static void Edit(
			unsigned int id,
			const std::string &name,
			const std::string group,
			const std::string &description,
			unsigned int workflow_id,
			const std::string &user,
			const std::string &host,
			const std::string &parameters);
		
		static void Delete(unsigned int id);
		static void List(QueryResponse *response);
		
		static bool HandleQuery(const User &user, XMLQuery *query, QueryResponse *response);
};

}

#endif
