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

#ifndef _GARBAGECOLLECTOR_H_
#define _GARBAGECOLLECTOR_H_

#include <Thread/WaiterThread.h>
#include <API/APIAutoInit.h>

#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>

class GarbageCollector: public WaiterThread, public APIAutoInit
{
	public:
		typedef int (*t_purge) (time_t now);
		
	private:
		bool enable;
		int delay;
		int interval;
		int limit;
		int workflowinstance_retention;
		int logs_retention;
		int logsapi_retention;
		int logsnotifications_retention;
		int uniqueaction_retention;
		std::string dbname;
		
		std::thread gc_thread_handle;
		
		std::vector<t_purge> purge_handlers;
		
		static GarbageCollector *instance;
		
	public:
		GarbageCollector();
		~GarbageCollector();
		
		static GarbageCollector *GetInstance() { return instance; }
		
		void APIReady();
		void RegisterPurgeHandler(t_purge handler);
		
		void WaitForShutdown(void);
	
	private:
		static void *gc_thread(GarbageCollector *gc);
		
		int purge(time_t now);
};

#endif
