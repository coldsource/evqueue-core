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

#include <Scheduler.h>
#include <Logger.h>

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <mysql/mysql.h>

#include <chrono>
#include <atomic>

using namespace std;

Scheduler::Scheduler()
{
	self_name = "";
	
	number_of_events = 0;
	first_event = 0;
	
	is_shutting_down = false;
	thread_is_running = false;
}

Scheduler::~Scheduler()
{
	// Delete all events
	Event *tmp;
	Event **event = &first_event;
	while(*event)
	{
		tmp = *event;
		*event = (*event)->next_event;
		
		delete tmp;
	}
}

void Scheduler::InsertEvent(Event *new_event)
{
	unique_lock<mutex> llock(scheduler_mutex);
	
	bool wake_up_retry_thread = false;
	
	if (first_event == 0)
	{
		new_event->next_event = 0;
		first_event = new_event;
		number_of_events = 0;
		
		// Create our thread
		if(!thread_is_running)
		{
			thread_is_running = true;
			
			retry_thread_handle = thread(Scheduler::retry_thread,this);
		}
	}
	else if (new_event->scheduled_at < first_event->scheduled_at)
	{
		new_event->next_event = first_event;
		first_event = new_event;
		wake_up_retry_thread = true;
	}
	else
	{
		Event *curr_event = first_event;
		while ( curr_event->next_event != 0 && new_event->scheduled_at >= curr_event->next_event->scheduled_at )
			curr_event = curr_event->next_event;
		new_event->next_event = curr_event->next_event;
		curr_event->next_event = new_event;
	}
	
	number_of_events++;
	
	if (wake_up_retry_thread)
		sleep_cond.notify_one(); // Wake up retry_thread because a new task starts sooner
}

void Scheduler::Flush(bool use_filter)
{
	unique_lock<mutex> llock(scheduler_mutex);
	
	bool wake_up_retry_thread = false;
	
	if(number_of_events==0)
	{
		Logger::Log(LOG_NOTICE,"%s is empty",self_name);
		return; // Nothing to do
	}
	
	// Execute all tasks
	Event *tmp;
	Event **event = &first_event;
	while(*event)
	{
		if(!use_filter || flush_filter(*event))
		{
			if(event==&first_event)
				wake_up_retry_thread = true; // We must change wait schedule if the first event is removed
			
			tmp = *event;
			*event = (*event)->next_event;
			
			event_removed(tmp,FLUSH);
			
			number_of_events--;
		}
		else
			event = &((*event)->next_event);
	}
	
	if(wake_up_retry_thread)
		sleep_cond.notify_one();  // Release retry_thread if it is locked
	
	Logger::Log(LOG_NOTICE,"%s flushed",self_name);
}

void Scheduler::Shutdown(void)
{
	unique_lock<mutex> llock(scheduler_mutex);
	
	is_shutting_down = true;
	
	if(number_of_events>0)
		sleep_cond.notify_one(); // Release retry_thread if it is locked
}

void Scheduler::WaitForShutdown(void)
{
	if(number_of_events==0)
		return; // retry_thread not launched, nothing to wait on
	
	retry_thread_handle.join();
}

void *Scheduler::retry_thread(Scheduler *scheduler)
{
	Event *event;
	
	mysql_thread_init();
	
	Logger::Log(LOG_INFO,"%s started",scheduler->self_name);
	
	while(1)
	{
		time_t curr_time = time(0);
		
		while(event = scheduler->shift_event(curr_time))
		{
			scheduler->event_removed(event,ALARM);
		}
		
		unique_lock<mutex> llock(scheduler->scheduler_mutex);
		
		if (scheduler->number_of_events == 0)
		{
			scheduler->thread_is_running = false;
			
			scheduler->retry_thread_handle.detach(); // We are not in shutdown status, we won't be joined
			
			mysql_thread_end();
			
			Logger::Log(LOG_INFO,"%s exited",scheduler->self_name);
			return 0;
		}
		else
		{
			time_t time_to_sleep = scheduler->get_sleep_time(curr_time);
			if (time_to_sleep > 0) {
				auto abstime = chrono::system_clock::now() + chrono::seconds(time_to_sleep);
				
				if(scheduler->is_shutting_down)
				{
					Logger::Log(LOG_NOTICE,"Shutdown in progress exiting %s",scheduler->self_name);
					
					mysql_thread_end();
					
					return 0; // Shutdown in progress
				}
				
				scheduler->sleep_cond.wait_until(llock,abstime);
				
				if(scheduler->is_shutting_down)
				{
					Logger::Log(LOG_NOTICE,"Shutdown in progress exiting %s",scheduler->self_name);
					
					mysql_thread_end();
					
					return 0; // Shutdown in progress
				}
			}
		}
	}
}

bool Scheduler::flush_filter(Scheduler::Event *e)
{
	return true;
}

Scheduler::Event *Scheduler::shift_event(time_t curr_time)
{
	unique_lock<mutex> llock(scheduler_mutex);
	
	if ( first_event != 0 && first_event->scheduled_at <= curr_time )
	{
		Event *event = first_event;
		first_event = first_event->next_event;
		number_of_events--;
		
		return event;
	}
	
	return 0;
}


time_t Scheduler::get_sleep_time(time_t curr_time) {
	return first_event->scheduled_at - curr_time;
}
