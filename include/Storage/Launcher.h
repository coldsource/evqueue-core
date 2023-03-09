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
class DB;

namespace Storage
{

class Launcher
{
	unsigned int launcher_id;
	std::string name;
	std::string group;
	std::string description;
	unsigned int workflow_id;
	std::string user;
	std::string host;
	std::string parameters;
	
	static void create_edit_check(
		const std::string &name,
		const std::string group,
		const std::string &description,
		unsigned int workflow_id,
		const std::string &user,
		const std::string &host,
		const std::string &parameters);
	
	public:
		Launcher(DB *db,unsigned int launcher_id);
		
		unsigned int GetID() const { return launcher_id; }
		const std::string &GetName() const { return name; }
		const std::string &GetGroup() const { return group; }
		const std::string &GetDescription() const { return description; }
		unsigned int GetWorkflowID() const { return workflow_id; }
		const std::string &GetUser() const { return user; }
		const std::string &GetHost() const { return host; }
		const std::string &GetParameters() const { return parameters; }
		
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
		
		static bool HandleQuery(const User &user, XMLQuery *query, QueryResponse *response);
};

}

#endif
