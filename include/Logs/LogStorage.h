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

#ifndef _LOGSTORAGE_H_
#define _LOGSTORAGE_H_

#include <string>
#include <map>
#include <vector>
#include <mutex>

class LogStorage
{
	std::mutex lock;
	
	std::map<std::string, unsigned int> pack_str_id;
	
	public:
		LogStorage();
		
		void StoreLog(unsigned int channel_id, std::map<std::string, std::string> &std_fields, std::map<std::string, std::string> &custom_fields);
	
	private:
		unsigned int pack(const std::string &str);
		void add_query_field_str(std::string &query, const std::string &name, std::map<std::string, std::string> &fields, std::string *val, std::vector<void *> &vals);
		void add_query_field_int(std::string &query, const std::string &name, std::map<std::string, std::string> &fields, int *val, std::vector<void *> &vals, bool need_pack);
};

#endif
