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
			INSTANCE_TERMINATED,
			QUEUE_ENQUEUE,
			QUEUE_DEQUEUE,
			QUEUE_EXECUTE,
			QUEUE_TERMINATE
		};
		
	private:
		struct st_subscription
		{
			unsigned int instance_filter;
			std::string api_cmd;
		};
		
		static Events *instance;
		
		std::mutex lock;
		
		std::map<struct lws *, std::set<std::string>> events;
		std::map<en_types, std::multimap<struct lws *, st_subscription>> subscriptions;
		
		struct lws_context *ws_context;
		
		en_types get_type(const std::string &type_str);
	
	public:
		Events(struct lws_context *ws_context);
		
		static Events *GetInstance() { return instance; }
		
		void Subscribe(const std::string &type_str, struct lws *wsi, unsigned int instance_filter, const std::string &api_cmd);
		void Unsubscribe(const std::string &type_str, struct lws *wsi, unsigned int instance_filter);
		void UnsubscribeAll(struct lws *wsi);
		
		void Create(en_types type, unsigned int instance_id);
		bool Get(struct lws *wsi, std::string &api_cmd);
};

#endif
