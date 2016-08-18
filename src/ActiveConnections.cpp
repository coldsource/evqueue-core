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

#include <ActiveConnections.h>
#include <handle_connection.h>

ActiveConnections::ActiveConnections()
{
	is_shutting_down = false;
	pthread_mutex_init(&lock, NULL);
}

void ActiveConnections::RegisterConnection(int s)
{
	pthread_mutex_lock(&lock);
	
	int *sp = new int;
	*sp = s;
	
	pthread_t thread;
	pthread_attr_t thread_attr;
	pthread_attr_init(&thread_attr);
	pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&thread, &thread_attr, handle_connection, sp);
	
	active_threads.insert(thread);
	
	pthread_mutex_unlock(&lock);
}

void ActiveConnections::UnregisterConnection(pthread_t thread)
{
	pthread_mutex_lock(&lock);
	
	if(is_shutting_down)
	{
		pthread_mutex_unlock(&lock);
		
		return;
	}
	
	pthread_detach(thread);
	active_threads.erase(thread);
	
	pthread_mutex_unlock(&lock);
}

void ActiveConnections::Shutdown(void)
{
	pthread_mutex_lock(&lock);
	
	is_shutting_down = true;
	
	pthread_mutex_unlock(&lock);
}

void ActiveConnections::WaitForShutdown()
{
	pthread_mutex_lock(&lock);
	
	pthread_mutex_unlock(&lock);
}