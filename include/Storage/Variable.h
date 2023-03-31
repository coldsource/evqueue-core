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
	nlohmann::json jvalue;
	
	void init(const DB &db);
	
	void check_path(const std::string &path) const;
	void check_name(const std::string &name) const;
	void check_value(const std::string &value) const;
	void check_value_type(const nlohmann::json &j) const;
	
	public:
		Variable();
		Variable(unsigned int id);
		Variable(const std::string &path, const std::string &name);
		
		void Update();
		void Unset();
		
		unsigned int GetID() const { return id; }
		const std::string &GetPath() const { return path; }
		const std::string &GetType() const { return type; }
		const std::string &GetStructure() const { return structure; }
		const std::string &GetName() const { return name; }
		std::string GetValue() const { return jvalue.dump(); }
		nlohmann::json GetJsonValue() const { return jvalue; }
		
		nlohmann::json Head() const;
		nlohmann::json Tail() const;
		
		void SetPath(const std::string &path);
		void SetType(const std::string &type);
		void SetStructure(const std::string &structure);
		void SetName(const std::string &name);
		void SetValue(const std::string &value);
		void SetValue(const nlohmann::json &jvalue);
		
		void Append(const std::string &value);
		void Append(const std::string &key, const std::string &value);
		void Add(int n);
};

}

#endif
