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

#ifndef _EVENTS_H_
#define _EVENTS_H_

#include <libwebsockets.h>

#include <thread>
#include <mutex>
#include <set>
#include <map>

class Events
{
	public:
		enum en_types
		{
			NONE,
			INSTANCE_STARTED,
			INSTANCE_TERMINATED
		};
	
	private:
		static Events *instance;
		
		std::mutex lock;
		
		std::set<en_types> events;
		std::map<en_types, std::set<struct lws *>> subscribers;
		
		struct lws_context *ws_context;
	
	public:
		Events(struct lws_context *ws_context);
		
		static Events *GetInstance() { return instance; }
		
		void Subscribe(en_types type, struct lws *wsi);
		void Unsubscribe(en_types type, struct lws *wsi);
		
		void Create(en_types type);
		en_types Get();
};

#endif
