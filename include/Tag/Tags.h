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

#ifndef _TAGS_H_
#define _TAGS_H_

#include <API/APIObjectList.h>

class User;
class Tag;
class XMLQuery;
class QueryResponse;

class Tags:public APIObjectList<Tag>
{
	static Tags *instance;
	
	public:
		
		Tags();
		~Tags();
		
		static Tags *GetInstance() { return instance; }
		
		void Reload(bool notify = true);
		
		static bool HandleQuery(const User &user, XMLQuery *query, QueryResponse *response);
};


#endif
