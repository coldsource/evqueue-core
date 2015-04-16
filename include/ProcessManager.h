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
#include <pthread.h>

class ProcessManager
{
	private:
		struct st_msgbuf
		{
			long type;
			
			struct {
				pid_t pid;
				pid_t tid;
				char retcode;
			} mtext;
		};
		
		int msgqid;
		
		const char *logs_directory;
		unsigned int logs_directory_len;
		char *log_filename;
		bool logs_delete;
		
		static volatile bool is_shutting_down;
		
		pthread_t forker_thread_handle;
		pthread_t gatherer_thread_handle;
		
	public:
		ProcessManager();
		~ProcessManager();
		
		pid_t ExecuteTask(const char *binary);
		static void *Fork(void *context);
		static void *Gather(void *context);
		
		void Shutdown(void);
		void WaitForShutdown(void);
};

#endif
