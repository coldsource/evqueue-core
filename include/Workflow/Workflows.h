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

#ifndef _WORKFLOWS_H_
#define _WORKFLOWS_H_

#include <APIObjectList.h>

#include <map>
#include <string>

class Workflow;
class SocketQuerySAX2Handler;
class QueryResponse;
class User;

class Workflows:public APIObjectList<Workflow>
{
	private:
		static Workflows *instance;
		
	public:
		
		Workflows();
		~Workflows();
		
		static Workflows *GetInstance() { return instance; }
		
		void Reload(bool notify = true);
		
		static bool HandleQuery(const User &user, SocketQuerySAX2Handler *saxh, QueryResponse *response);
};

#endif
