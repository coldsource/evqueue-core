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

#ifndef _TAG_H_
#define _TAG_H_

#include <string>

class DB;
class SocketQuerySAX2Handler;
class QueryResponse;
class User;

class Tag
{
	unsigned int id;
	std::string label;
	
	public:
		Tag();
		Tag(DB *db,unsigned int id);
		
		static void InitAnonymous();
		
		const unsigned int GetID() const { return id; }
		const std::string &GetLabel() const { return label; }
		
		static void Get(unsigned int id, QueryResponse *response);
		static unsigned int Create(const std::string &label);
		static void Edit(unsigned int id, const std::string &label);
		static void Delete(unsigned int id);
		
		static bool HandleQuery(const User &user, SocketQuerySAX2Handler *saxh, QueryResponse *response);
};

#endif
