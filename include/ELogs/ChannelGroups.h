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

#ifndef _CHANNELGROUPS_H_
#define _CHANNELGROUPS_H_

#include <API/APIObjectList.h>
#include <API/APIAutoInit.h>
#include <ELogs/ChannelGroup.h>

#include <map>
#include <string>

class User;
class XMLQuery;
class QueryResponse;

namespace ELogs
{

class ChannelGroups:public APIObjectList<ChannelGroup>, public APIAutoInit
{
	static ChannelGroups *instance;
	
	public:
		
		ChannelGroups();
		virtual ~ChannelGroups();
		
		static ChannelGroups *GetInstance() { return instance; }
		
		void Reload(bool notify = true);
		
		static bool HandleQuery(const User &user, XMLQuery *query, QueryResponse *response);
		static void HandleReload(bool notify);
};

}


#endif
