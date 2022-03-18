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
	
	static LogStorage *instance;
	
	public:
		LogStorage();
		
		static LogStorage *GetInstance() { return instance; }
		
		void StoreLog(unsigned int channel_id, std::map<std::string, std::string> &std_fields, std::map<std::string, std::string> &custom_fields);
		
		int PackInteger(const std::string &str);
		std::string UnpackInteger(int i);
		std::string PackIP(const std::string &ip);
		std::string UnpackIP(const std::string &bin_ip);
		int PackCrit(const std::string &str);
		std::string UnpackCrit(int i);
		unsigned int PackString(const std::string &str);
	
	private:
		void add_query_field(int type, std::string &query, const std::string &name, std::map<std::string, std::string> &fields, void *val, std::vector<void *> &vals);
};

#endif
