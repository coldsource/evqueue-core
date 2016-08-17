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
#include <pthread.h>
#include <mysql/mysql.h>

Scheduler::Scheduler()
{
	self_name = "";
	
	number_of_events = 0;
	first_event = 0;
	
	is_shutting_down = false;
	thread_is_running = false;
	
	pthread_mutex_init(&mutex,NULL);
	pthread_cond_init(&sleep_cond,NULL);
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
	pthread_mutex_lock(&mutex);
	
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
			
			pthread_attr_t attr;
			pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
			pthread_create(&retry_thread_handle, &attr, &Scheduler::retry_thread,this);
			pthread_setname_np(retry_thread_handle,self_name);
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
		pthread_cond_signal(&sleep_cond); // Wake up retry_thread because a new task starts sooner
	
	pthread_mutex_unlock(&mutex);
}

void Scheduler::Flush(bool use_filter)
{
	pthread_mutex_lock(&mutex);
	
	bool wake_up_retry_thread = false;
	
	if(number_of_events==0)
	{
		pthread_mutex_unlock(&mutex);
		
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
		pthread_cond_signal(&sleep_cond); // Release retry_thread if it is locked
	
	pthread_mutex_unlock(&mutex);
	
	Logger::Log(LOG_NOTICE,"%s flushed",self_name);
}

void Scheduler::Shutdown(void)
{
	pthread_mutex_lock(&mutex);
	
	is_shutting_down = true;
	
	if(number_of_events>0)
		pthread_cond_signal(&sleep_cond); // Release retry_thread if it is locked
	
	pthread_mutex_unlock(&mutex);
}

void Scheduler::WaitForShutdown(void)
{
	if(number_of_events==0)
		return; // retry_thread not launched, nothing to wait on
	
	pthread_join(retry_thread_handle,0);
}

void *Scheduler::retry_thread( void *context )
{
	Scheduler *scheduler = (Scheduler *)context;
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
		
		pthread_mutex_lock(&scheduler->mutex);
		
		if (scheduler->number_of_events == 0)
		{
			scheduler->thread_is_running = false;
			
			pthread_mutex_unlock(&scheduler->mutex);
			pthread_detach(scheduler->retry_thread_handle); // We are not in shutdown status, we won't be joined
			
			mysql_thread_end();
			
			Logger::Log(LOG_INFO,"%s exited",scheduler->self_name);
			return 0;
		}
		else
		{
			time_t time_to_sleep = scheduler->get_sleep_time(curr_time);
			if (time_to_sleep > 0) {
				timespec abstime;
				clock_gettime(CLOCK_REALTIME,&abstime);
				abstime.tv_sec += time_to_sleep;
				
				if(scheduler->is_shutting_down)
				{
					Logger::Log(LOG_NOTICE,"Shutdown in progress exiting %s",scheduler->self_name);
					
					mysql_thread_end();
					
					return 0; // Shutdown in progress
				}
				
				pthread_cond_timedwait(&scheduler->sleep_cond,&scheduler->mutex,&abstime);
				
				if(scheduler->is_shutting_down)
				{
					Logger::Log(LOG_NOTICE,"Shutdown in progress exiting %s",scheduler->self_name);
					
					mysql_thread_end();
					
					return 0; // Shutdown in progress
				}
				
				pthread_mutex_unlock(&scheduler->mutex);
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
	pthread_mutex_lock(&mutex);
	
	if ( first_event != 0 && first_event->scheduled_at <= curr_time )
	{
		Event *event = first_event;
		first_event = first_event->next_event;
		number_of_events--;
		
		pthread_mutex_unlock(&mutex);
		return event;
	}
	
	pthread_mutex_unlock(&mutex);
	return 0;
}


time_t Scheduler::get_sleep_time(time_t curr_time) {
	return first_event->scheduled_at - curr_time;
}
