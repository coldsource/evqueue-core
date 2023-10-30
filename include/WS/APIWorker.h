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

#ifndef _APIWORKER_H_
#define _APIWORKER_H_

#include <queue>
#include <string>

#include <libwebsockets.h>

#include <Thread/ConsumerThread.h>

class APISession;
class APICmdBuffer;

class APIWorker: public ConsumerThread
{
	public:
		struct st_cmd
		{
			struct lws *wsi;
			std::string cmd;
		};
	
	private:
		std::string cmd;
		
		struct lws_context *ws_context;
		struct lws *wsi;
		APISession *session;
	
	protected:
		void get();
		void process();
	
	public:
		APIWorker(APICmdBuffer *buffer, struct lws_context *ws_context): ConsumerThread((ProducerThread *)buffer)
		{
			this->ws_context = ws_context;
			
			start();
		}
		
		virtual ~APIWorker()
		{
		}
};

#endif
