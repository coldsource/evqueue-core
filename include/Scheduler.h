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

#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#include <time.h>
#include <pthread.h>

class Scheduler
{
	protected:
		struct Event
		{
			time_t scheduled_at;
			Event *next_event;
			
			virtual ~Event() {};
		};
		
		enum event_reasons {ALARM, FLUSH};
		
		const char *self_name;
		
		unsigned int number_of_events;
		Event *first_event;
		
		pthread_t retry_thread_handle;
		pthread_mutex_t mutex;
		pthread_cond_t sleep_cond;
		
		bool is_shutting_down;
		bool thread_is_running;
	
	public:
		Scheduler();
		virtual ~Scheduler();
		
		void InsertEvent(Event *new_event);
		void Flush(bool use_filter = false);
		
		void Shutdown(void);
		void WaitForShutdown(void);
	
	private:
		static void *retry_thread(void *context);
		
		Event *shift_event( time_t curr_time );
		time_t get_sleep_time( time_t curr_time );
		
	protected:
		virtual void event_removed(Event *e, event_reasons reason) = 0;
		
		virtual bool flush_filter(Event *e);
};

#endif
