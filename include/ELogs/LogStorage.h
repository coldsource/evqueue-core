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

#include <API/APIAutoInit.h>
#include <Thread/ConsumerThread.h>
#include <Thread/ProducerThread.h>

#include <string>
#include <map>
#include <vector>
#include <queue>
#include <regex>

class DB;

namespace ELogs
{

class Channel;
class Field;

class LogStorage: public APIAutoInit, public ConsumerThread, public ProducerThread
{
	std::map<std::string, unsigned int> pack_str_id;
	std::map<unsigned int, std::string> pack_id_str;
	
	int bulk_size;
	std::regex channel_regex;
	std::queue<std::string> logs;
	std::vector<std::string> to_insert_logs;
	size_t max_queue_size;
	
	static LogStorage *instance;
	
	bool is_shutting_down = false;
	
	unsigned long long next_log_id;
	unsigned int last_partition_days;
	
	DB *storage_db;
	
	public:
		LogStorage();
		virtual ~LogStorage();
		
		static LogStorage *GetInstance() { return instance; }
		
		void Log(const std::string &str);
		
		unsigned int PackString(const std::string &str);
		std::string UnpackString(int i);
	
	protected:
		bool data_available();
		void init_thread();
		void release_thread();
		void get();
		void process();
	
	private:
		void log(const std::vector<std::string> &logs);
		void store_log(const Channel &channel, const std::map<std::string, std::string> &group_fields, const std::map<std::string, std::string> &channel_fields);
		void log_value(unsigned long long log_id, const Field &field, const std::string &date, const std::string &value);
		void create_partition(const std::string &date);
};

}

#endif
