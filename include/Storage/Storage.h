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

class XMLQuery;
class QueryResponse;
class User;

class Storage
{
	unsigned int id;
	std::string label;
	
	public:
		Storage();
		
		static void Get(const std::string &ns, const std::string &type, const std::string label, QueryResponse *response);
		static void GetType(const std::string &ns, const std::string &type, QueryResponse *response);
		static unsigned int Set(const std::string &ns, const std::string &type, const std::string label, const std::string value);
		
		static bool HandleQuery(const User &user, XMLQuery *query, QueryResponse *response);
};

#endif
