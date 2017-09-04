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

#ifndef _PROCESS_MANAGER_H_
#define _PROCESS_MANAGER_H_

#include <sys/types.h>

#include <string>
#include <vector>
#include <thread>

class ProcessManager
{
	private:
		int msgqid;
		
		static std::string logs_directory;
		static bool logs_delete;
		static int log_maxsize;
		
		static volatile bool is_shutting_down;
		
		std::thread forker_thread_handle;
		std::thread gatherer_thread_handle;
		
	public:
		ProcessManager();
		~ProcessManager();
		
		static void *Fork(ProcessManager *pm);
		static void *Gather(ProcessManager *pm);
		
		void Shutdown(void);
		void WaitForShutdown(void);
		
		static pid_t ExecuteTask(
			const std::string &task_name,
			std::vector<std::string> &parameters_name,
			std::vector<std::string> &parameters_value,
			const std::string &stdin_parameter,
			pid_t tid,
			const std::string &host,
			const std::string &user
			);
	
	private:
		static int open_log_file(int tid, int log_fileno);
		static char *read_log_file(pid_t pid,pid_t tid,int log_fileno);
};

#endif
