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

#ifndef _DATAPIPER_H_
#define _DATAPIPER_H_

#include <thread>
#include <mutex>
#include <map>
#include <string>

class DataPiper
{
	struct st_data
	{
		std::string data;
		size_t written;
	};
	
	static DataPiper *instance;
	
	bool is_shutting_down = false;
	
	std::thread dp_thread_handle;
	std::mutex lock;
	
	// Self pipe, used to woke poll()
	int self_pipe[2];
	
	// Associates file descriptor (pipe) to data to be sent
	std::map<int, st_data> pipe_data;
	
	static void dp_thread(DataPiper *dp);
	
	public:
		DataPiper();
		~DataPiper();
		
		static DataPiper *GetInstance() { return instance; }
		
		void PipeData(int fd, const std::string &data);
		
		void Shutdown();
		void WaitForShutdown();
};

#endif
