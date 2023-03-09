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

#ifndef _LAUNCHERS_H_
#define _LAUNCHERS_H_

#include <API/APIObjectList.h>
#include <API/APIAutoInit.h>
#include <Storage/Launcher.h>

#include <map>
#include <string>

class User;
class XMLQuery;
class QueryResponse;

namespace Storage
{

class Launchers:public APIObjectList<Launcher>, public APIAutoInit
{
	static Launchers *instance;
	
	public:
		
		Launchers();
		~Launchers();
		
		static Launchers *GetInstance() { return instance; }
		
		static void List(QueryResponse *response, const User &user);
		
		void Reload(bool notify = true);
		
		static bool HandleQuery(const User &user, XMLQuery *query, QueryResponse *response);
		static void HandleReload(bool notify);
};

}


#endif
