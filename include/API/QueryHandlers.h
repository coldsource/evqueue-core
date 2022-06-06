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
#include <vector>

class XMLQuery;
class QueryResponse;
class User;
class APIAutoInit;
class QueryHandlers;

typedef bool (*t_query_handler)(const User &user, XMLQuery *query, QueryResponse *response);
typedef void (*t_reload_handler)(bool notify);
typedef APIAutoInit * (*t_query_handler_init)(QueryHandlers *qh);

class QueryHandlers
{
	static QueryHandlers *instance;
	
	std::map<std::string,t_query_handler> query_handlers;
	std::map<std::string,t_reload_handler> reload_handlers;
	std::map<std::string, std::string> modules;
	
	std::vector<APIAutoInit *> auto_init_ptr;
	std::vector<t_query_handler_init> auto_init;
	
	public:
		QueryHandlers();
		~QueryHandlers();
		
		static QueryHandlers *GetInstance();
		
		const std::map<std::string, std::string> &GetModules() { return modules; }
		
		bool RegisterInit(t_query_handler_init init);
		void AutoInit();
		
		void RegisterHandler(const std::string &type, t_query_handler handler);
		void RegisterReloadHandler(const std::string &type, t_reload_handler handler);
		void RegisterModule(const std::string &name, const std::string &version);
		
		bool HandleQuery(const User &user, const std::string &type, XMLQuery *query, QueryResponse *response);
		bool Reload(const std::string &module, bool notify);
};

#endif
