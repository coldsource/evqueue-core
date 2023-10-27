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

#ifndef _APICMDBUFFER_H_
#define _APICMDBUFFER_H_

#include <queue>
#include <mutex>

#include <Thread/ProducerThread.h>
#include <WS/APIWorker.h>

class APICmdBuffer: public std::queue<APIWorker::st_cmd>, public ProducerThread
{
	protected:
		bool data_available() { return size()>0; }
	
	public:
		void Received(const APIWorker::st_cmd &cmd)
		{
			std::unique_lock<std::mutex> llock(lock);
			
			push(cmd);
			produced();
		}
};

#endif
