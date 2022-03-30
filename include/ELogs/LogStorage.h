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
#include <queue>
#include <mutex>
#include <regex>
#include <condition_variable>
#include <thread>

class DB;

namespace ELogs
{

class Channel;
class Field;

class LogStorage
{
	std::mutex lock;
	std::condition_variable logs_queued;
	
	std::thread ls_thread_handle;
	
	std::map<std::string, unsigned int> pack_str_id;
	std::map<unsigned int, std::string> pack_id_str;
	
	int bulk_size;
	std::regex channel_regex;
	std::queue<std::string> logs;
	size_t max_queue_size;
	
	static LogStorage *instance;
	
	bool is_shutting_down = false;
	
	public:
		LogStorage();
		
		static LogStorage *GetInstance() { return instance; }
		
		void Shutdown(void);
		void WaitForShutdown(void);
		
		void Log(const std::string &str);
		
		unsigned int PackString(const std::string &str);
		std::string UnpackString(int i);
	
	private:
		static void *ls_thread(LogStorage *ls);
		
		void log(const std::vector<std::string> &logs);
		void store_log(DB *db, const Channel &channel, const std::map<std::string, std::string> &group_fields, const std::map<std::string, std::string> &channel_fields);
		void log_value(DB *db, unsigned long long log_id, const Field &field, const std::string &date, const std::string &value);
};

}

#endif
