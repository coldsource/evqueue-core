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

#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#include <string>

#include <nlohmann/json.hpp>

class XMLQuery;
class QueryResponse;
class User;
class DB;

namespace Storage
{

class Display
{
	unsigned int display_id;
	std::string name;
	std::string group;
	std::string path;
	std::string order;
	std::string item_title;
	std::string item_content;
	
	static void create_edit_check(
		const std::string &name,
		const std::string &group,
		const std::string &path,
		const std::string &order,
		const std::string &item_title,
		const std::string &item_content);
	
	public:
		Display(DB *db,unsigned int display_id);
		
		unsigned int GetID() const { return display_id; }
		const std::string &GetName() const { return name; }
		const std::string &GetGroup() const { return group; }
		const std::string &GetPath() const { return path; }
		const std::string &GetOrder() const { return order; }
		const std::string &GetItemTitle() const { return item_title; }
		const std::string &GetItemContent() const { return item_content; }

		
		static void Get(unsigned int id, QueryResponse *response);
		static unsigned int Create(
			const std::string &name,
			const std::string &group,
			const std::string &path,
			const std::string &order,
			const std::string &item_title,
			const std::string &item_content);
		
		static void Edit(
			unsigned int id,
			const std::string &name,
			const std::string &group,
			const std::string &path,
			const std::string &order,
			const std::string &item_title,
			const std::string &item_content);
		
		static void Delete(unsigned int id);
		
		static bool HandleQuery(const User &user, XMLQuery *query, QueryResponse *response);
};

}

#endif
