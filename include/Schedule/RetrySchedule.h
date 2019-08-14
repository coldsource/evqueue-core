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

#ifndef _RETRYSCHEDULE_H_
#define _RETRYSCHEDULE_H_

#include <string>

class DB;
class SocketQuerySAX2Handler;
class QueryResponse;
class User;

class RetrySchedule
{
	unsigned int id;
	std::string name;
	std::string schedule_xml;
		
	public:
		RetrySchedule() {}
		RetrySchedule(DB *db,const std::string &name);
		
		inline unsigned int GetID() { return id; }
		inline const std::string &GetName() const { return name; }
		inline std::string &GetXML() { return schedule_xml; }
		
		static bool CheckRetryScheduleName(const std::string &retry_schedule_name);
		
		static void Get(unsigned int id, QueryResponse *response);
		static unsigned int Create(const std::string &name, const std::string &base64);
		static void Edit(unsigned int id,const std::string &name, const std::string &base64);
		static void Delete(unsigned int id);
		
		static bool HandleQuery(const User &user, SocketQuerySAX2Handler *saxh, QueryResponse *response);
	
	private:
		static std::string create_edit_check(const std::string &name, const std::string &base64);
};

#endif
