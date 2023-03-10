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

#ifndef _VARIABLE_H_
#define _VARIABLE_H_

#include <string>

#include <nlohmann/json.hpp>

class DB;

namespace Storage
{

class Variable
{
	unsigned int id;
	std::string path;
	std::string type;
	std::string structure;
	std::string name;
	std::string value;
	nlohmann::json jvalue;
	
	void init(const DB &db);
	
	public:
		Variable() {}
		Variable(unsigned int id);
		Variable(const std::string &path, const std::string &name);
		
		unsigned int GetID() const { return id; }
		const std::string &GetPath() const { return path; }
		const std::string &GetType() const { return type; }
		const std::string &GetStructure() const { return structure; }
		const std::string &GetName() const { return name; }
		nlohmann::json GetValue() const { return jvalue; }
};

}

#endif
