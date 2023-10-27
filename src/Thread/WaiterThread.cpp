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

#include <Thread/WaiterThread.h>

using namespace std;

void WaiterThread::start()
{
	thread_handle = thread(thread_main, this);
}

bool WaiterThread::wait(int seconds)
{
	while(true)
	{
		unique_lock<mutex> llock(wait_lock);
			
		cv_status ret;
		if(!is_shutting_down)
			ret = shutdown_requested.wait_for(llock, chrono::seconds(seconds));
		
		llock.unlock();
		
		if(is_shutting_down)
			return false;
		
		if(ret==cv_status::timeout)
			return true;
		
		// Suprious interrupt, continue waiting
	}
}

void WaiterThread::thread_main(WaiterThread *ptr)
{
	ptr->main();
}

void WaiterThread::Shutdown(void)
{
	if(thread_handle.get_id()==thread::id())
		return; // Thread not yet started, nothing to do
	
	wait_lock.lock();
	is_shutting_down = true;
	shutdown_requested.notify_one();
	wait_lock.unlock();
	
	thread_handle.join();
}
