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

#ifndef _FIELD_H_
#define _FIELD_H_

#include <string>

class XMLQuery;
class QueryResponse;
class User;
class DB;

class Field
{
	public:
		enum en_type
		{
			NONE,
			CHAR,
			INT,
			IP,
			PACK,
			TEXT
		};
	
	private:
		unsigned int id;
		std::string name;
		en_type type;
	
	public:
		Field();
		Field(unsigned int id);
		Field(const Field &f);
		
		unsigned int GetID() const { return id; }
		const std::string GetName() const { return name; }
		en_type GetType() const { return type; }
		
		static en_type StringToFieldType(const std::string &str);
		static std::string FieldTypeToString(en_type type);
		
		static int PackCrit(const std::string &str);
		static std::string UnpackCrit(int i);
};

#endif
