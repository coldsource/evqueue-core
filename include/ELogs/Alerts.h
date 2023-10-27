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

#ifndef _ALERTS_H_
#define _ALERTS_H_

#include <API/APIObjectList.h>
#include <API/APIAutoInit.h>
#include <ELogs/Alert.h>
#include <Thread/WaiterThread.h>

#include <map>
#include <string>
#include <condition_variable>
#include <thread>

class User;
class XMLQuery;
class QueryResponse;

namespace ELogs
{

class Alerts:public APIObjectList<Alert>, public APIAutoInit, public WaiterThread
{
	static Alerts *instance;
	
	public:
		
		Alerts();
		virtual ~Alerts();
		
		void APIReady();
		
		static Alerts *GetInstance() { return instance; }
		
		void Reload(bool notify = true);
		
		static bool HandleQuery(const User &user, XMLQuery *query, QueryResponse *response);
		static void HandleReload(bool notify);
		
		static void HandleNotificationTypeDelete(unsigned int id);
	
	private:
		void main();
};

}

#endif
