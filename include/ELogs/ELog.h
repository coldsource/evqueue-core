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

#ifndef _ELOG_H_
#define _ELOG_H_

class XMLQuery;
class QueryResponse;
class User;
class DOMElement;
class DB;

#include <string>

namespace ELogs
{

class Fields;

class ELog
{
	private:
		static void query_fields(DB *db, unsigned long long id, const Fields &fields, DOMElement &node);
	
	public:
		static bool HandleQuery(const User &user, XMLQuery *query, QueryResponse *response);
		
		static void BuildSelectFrom(std::string &query_select, std::string &query_from, const Fields &fields);
		static void BuildSelectFromAppend(std::string &query_select, std::string &query_from, const Fields &fields);
};

}

#endif
