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
#include <vector>
#include <map>

class Events
{
	public:
		typedef int en_types;
		
	private:
		bool throttling;
		
		unsigned long long event_id = 0;
		
		unsigned int events_map_id = 0;
		std::map<std::string, en_types> events_map;
		
		struct st_subscription
		{
			unsigned int object_filter;
			std::string api_cmd;
			int external_id;
		};
		
		struct st_event
		{
			std::string api_cmd;
			int external_id;
			std::string object_id;
			unsigned long long event_id;
			
			bool operator==(const st_event &r)
			{
				return api_cmd==r.api_cmd && external_id==r.external_id;
			}
		};
		
		struct st_online_event
		{
			st_event event;
			bool need_resend = false;
			std::string object_ids;
		};
		
		static Events *instance;
		
		std::mutex lock;
		
		std::map<struct lws *, std::vector<st_event>> events;
		std::map<struct lws *, std::vector<st_online_event>> online_events;
		std::map<en_types, std::multimap<struct lws *, st_subscription>> subscriptions;
		
		struct lws_context *ws_context;
		
		en_types get_type(const std::string &type_str);
		
		void insert_event(struct lws *wsi, const st_event &event);
	
	public:
		Events();
		~Events();
		
		static Events *GetInstance() { return instance; }
		
		void SetContext(struct lws_context *ws_context);
		
		void RegisterEvent(const std::string name);
		void RegisterEvents(const std::vector<std::string> &names);
		
		void Subscribe(const std::string &type_str, struct lws *wsi, unsigned int object_filter, int external_id, const std::string &api_cmd);
		void Unsubscribe(const std::string &type_str, struct lws *wsi, unsigned int object_filter, int external_id);
		void UnsubscribeAll(struct lws *wsi);
		
		void Create(const std::string &type_str, unsigned int object_id = 0, struct lws *filter_wsi = 0, int filter_external_id = 0);
		bool Get(struct lws *wsi, int *external_id, std::string &object_id, unsigned long long *event_id, std::string &api_cmd);
		void Ack(struct lws *wsi, unsigned long long event_id);
};

#endif
