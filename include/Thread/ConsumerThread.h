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

#ifndef _CONSUMERTHREAD_H_
#define _CONSUMERTHREAD_H_

#include <mutex>
#include <condition_variable>
#include <thread>

class ProducerThread;

class ConsumerThread
{
	protected:
		ProducerThread *producer;
		
		std::thread th;
		
		bool is_shutting_down = 0;
		
		void start();
		virtual void get() = 0;
		virtual void process() = 0;
		virtual void init_thread() {}
		virtual void release_thread() {}
		
		static void main(ConsumerThread *consumer, ProducerThread *producer);
	
	public:
		ConsumerThread(ProducerThread *producer);
		
		void Shutdown();
};

#endif
