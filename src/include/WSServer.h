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

#include <libwebsockets.h>

class WSServer;
class User;
class APISession;
class Events;

class WSServer
{
	static WSServer *instance;
	
	bool is_cancelling = false;
	
	struct lws_context_creation_info info;
	struct lws_context *context;
	
	std::thread event_loop_thread;
	
	Events *events;
	
	public:
		struct per_session_data
		{
			APISession *session;
		};
		
		enum en_protocols
		{
			API,
			RUNNING_INSTANCES,
			TERMINATED_INSTANCES
		};
	
	public:
		WSServer();
		~WSServer();
		
		static WSServer *GetInstance() { return instance; }
		
		void Start();
		void Shutdown();
		void WaitForShutdown();
		
	public:
		static int callback_http( struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len );
		static int callback_minimal(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
	
	private:
		static void event_loop(WSServer *ws);
};

#endif
