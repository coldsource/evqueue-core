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

#ifndef _EVENTSWORKER_H_
#define _EVENTSWORKER_H_

#include <Thread/ConsumerThread.h>

#include <libwebsockets.h>

#include <string>

class Events;
class APISession;

class EventsWorker: public ConsumerThread
{
	struct lws_context *ws_context;
	
	struct lws *wsi;
	std::string api_cmd;
	int external_id;
	std::string object_id;
	unsigned long long event_id;
	APISession *session;

	protected:
		void get();
		void process();
		
	public:
		EventsWorker(Events *events, struct lws_context *ws_context): ConsumerThread((ProducerThread *)events)
		{
			this->ws_context = ws_context;
		}
		
		virtual ~EventsWorker()
		{
		}
};

#endif
