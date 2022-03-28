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

#ifndef _FIELDS_H_
#define _FIELDS_H_

#include <string>
#include <map>

#include <Logs/Field.h>

#include <nlohmann/json.hpp>

class XMLQuery;
class QueryResponse;
class User;
class DB;

class Fields
{
	public:
		enum en_type
		{
			GROUP,
			CHANNEL
		};
	
	private:
		unsigned int id;
		en_type type;
		
		std::string col_name;
		
		std::map<unsigned int, Field> fields;
		
	public:
		Fields(en_type type, unsigned int id);
		
		const std::map<unsigned int, Field> &GetMap() const { return fields; }
		
		void Update(const nlohmann::json &j);
};

#endif
