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

#include <QueryHandlers.h>

QueryHandlers *QueryHandlers::instance=0;

QueryHandlers::QueryHandlers(void)
{
	instance = this;
}

void QueryHandlers::RegisterHandler(const std::string &type, t_query_handler handler)
{
	// No mutexes are used, RegisterHandler must be called within one thread only
	handlers[type] = handler;
}

bool QueryHandlers::HandleQuery(const User &user, const std::string &type, SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	auto it = handlers.find(type);
	if(it==handlers.end())
		return false;
	
	return it->second(user, saxh,response);
}