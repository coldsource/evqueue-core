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
#include <condition_variable>
#include <thread>

class LogStorage
{
	std::mutex lock;
	std::condition_variable logs_queued;
	
	std::thread ls_thread_handle;
	
	std::map<std::string, unsigned int> pack_str_id;
	
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
		
		int PackInteger(const std::string &str);
		std::string UnpackInteger(int i);
		std::string PackIP(const std::string &ip);
		std::string UnpackIP(const std::string &bin_ip);
		int PackCrit(const std::string &str);
		std::string UnpackCrit(int i);
		unsigned int PackString(const std::string &str);
	
	private:
		static void *ls_thread(LogStorage *ls);
		
		void log(const std::string &str);
		void store_log(unsigned int channel_id, std::map<std::string, std::string> &std_fields, std::map<std::string, std::string> &custom_fields);
		void add_query_field(int type, std::string &query, const std::string &name, std::map<std::string, std::string> &fields, void *val, std::vector<void *> &vals);
};

#endif
