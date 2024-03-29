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

#ifndef _WSSERVER_H_
#define _WSSERVER_H_

#include <string>
#include <thread>
#include <mutex>
#include <vector>

#include <libwebsockets.h>

#include <WS/EventsWorker.h>
#include <WS/APIWorker.h>
#include <Thread/ThreadPool.h>
#include <WS/APICmdBuffer.h>

class WSServer;
class User;
class APISession;
class Events;

class WSServer
{
	static WSServer *instance;
	
	bool is_cancelling = false;
	
	bool ssl_enabled = false;
	
	struct lws_context_creation_info info;
	struct lws_context *context;
	
	std::thread ws_worker;
	
	Events *events;
	APICmdBuffer api_cmd_buffer;
	ThreadPool<EventsWorker> *events_pool;
	ThreadPool<APIWorker> *api_pool;
	
	public:
		struct per_session_data
		{
			APISession *session;
			std::string *cmd_buffer;
		};
		
		enum en_protocols
		{
			API,
			EVENTS,
		};
	
	public:
		WSServer();
		~WSServer();
		
		static WSServer *GetInstance() { return instance; }
		
		void Adopt(int fd);
		
		void Shutdown();
		
	public:
		static int callback_http( struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len );
		static int callback_evq(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
		
		static void event_loop();
};

#endif
