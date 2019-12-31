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

#ifndef _QUERYHANDLERS_H_
#define _QUERYHANDLERS_H_

#include <map>
#include <string>

class XMLQuery;
class QueryResponse;
class User;

typedef bool (*t_query_handler)(const User &user, XMLQuery *query, QueryResponse *response);

class QueryHandlers
{
	static QueryHandlers *instance;
	
	std::map<std::string,t_query_handler> handlers;
	
	public:
		QueryHandlers();
		
		static QueryHandlers *GetInstance() { return QueryHandlers::instance; }
		
		void RegisterHandler(const std::string &type, t_query_handler handler);
		bool HandleQuery(const User &user, const std::string &type, XMLQuery *query, QueryResponse *response);
};

#endif
