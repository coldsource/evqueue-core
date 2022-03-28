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

#ifndef _CHANNELGROUP_H_
#define _CHANNELGROUP_H_

#include <string>
#include <regex>
#include <map>

class XMLQuery;
class QueryResponse;
class User;
class DB;

#include <Logs/Fields.h>

class ChannelGroup
{
	private:
		unsigned int channel_group_id;
		std::string channel_group_name;
		Fields fields;
	
	public:
		ChannelGroup(DB *db,unsigned int channel_group_id);
		
		unsigned int GetID() const { return channel_group_id; }
		const std::string &GetName() const { return channel_group_name; }
		
		static bool CheckChannelGroupName(const std::string &channel_group_name);
		static void Get(unsigned int id, QueryResponse *response);
		static unsigned int Create(const std::string &name, const std::string &fields_str);
		static void Edit(unsigned int id, const std::string &name, const std::string &fields_str);
		static void Delete(unsigned int id);
		
		static bool HandleQuery(const User &user, XMLQuery *query, QueryResponse *response);
	
	private:
		static void create_edit_check(const std::string &name, const std::string &fields);
};

#endif
