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

#include <API/QueryHandlers.h>
#include <API/APIAutoInit.h>
#include <Logger/Logger.h>

QueryHandlers *QueryHandlers::instance=0;

QueryHandlers::QueryHandlers(void)
{
	instance = this;
}

QueryHandlers::~QueryHandlers(void)
{
	for(int i=0;i<auto_init_ptr.size();i++)
		delete auto_init_ptr[i];
}

QueryHandlers *QueryHandlers::GetInstance()
{
	if(!instance)
		new QueryHandlers();
	
	return instance;
}

void QueryHandlers::RegisterHandler(const std::string &type, t_query_handler handler)
{
	// No mutexes are used, RegisterHandler must be called within one thread only
	handlers[type] = handler;
}

bool QueryHandlers::RegisterInit(t_query_handler_init init)
{
	auto_init.push_back(init);
	return true;
}

void QueryHandlers::AutoInit()
{
	for(int i=0;i<auto_init.size();i++)
	{
		APIAutoInit *p = (auto_init[i])(this);
		if(p)
			auto_init_ptr.push_back(p);
	}
}

bool QueryHandlers::HandleQuery(const User &user, const std::string &type, XMLQuery *query, QueryResponse *response)
{
	auto it = handlers.find(type);
	if(it==handlers.end())
		return false;
	
	Logger::Log(LOG_DEBUG, "API : Found handler for group '"+type+"'");
	
	return it->second(user, query, response);
}
