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

#include <Thread/ConsumerThread.h>
#include <Thread/ProducerThread.h>

using namespace std;

void ConsumerThread::start()
{
	th = thread(main, this, producer);
}

void ConsumerThread::main(ConsumerThread *consumer, ProducerThread *producer)
{
	consumer->init_thread();
	
	unique_lock<mutex> llock(producer->lock);
	
	while(true)
	{
		producer->cond.wait(llock, [consumer, producer] { return (producer->data_available() || consumer->is_shutting_down); });
		
		if(consumer->is_shutting_down)
		{
			consumer->release_thread();
			return;
		}
		
		consumer->get();
		
		llock.unlock();
		
		consumer->process();
		
		llock.lock();
	}
}

ConsumerThread::ConsumerThread(ProducerThread *producer)
{
	this->producer = producer;
}

void ConsumerThread::Shutdown()
{
	is_shutting_down = true;
	
	producer->cond.notify_all();
	
	th.join();
}
