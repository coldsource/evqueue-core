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

#include <Events.h>

using namespace std;

Events *Events::instance = 0;

Events::Events(struct lws_context *ws_context)
{
	this->ws_context = ws_context;
	instance = this;
}

Events::en_types Events::get_type(const std::string &type_str)
{
	if(type_str=="INSTANCE_STARTED")
		return INSTANCE_STARTED;
	else if(type_str=="INSTANCE_TERMINATED")
		return INSTANCE_TERMINATED;
	
	return NONE;
}

void Events::Subscribe(const string &type_str, struct lws *wsi, unsigned int instance_filter, const string &api_cmd)
{
	unique_lock<mutex> llock(lock);
	
	en_types type = get_type(type_str);
	
	subscriptions[type].insert(pair<struct lws *, st_subscription>(wsi,{instance_filter, api_cmd}));
}

void Events::Unsubscribe(const string &type_str, struct lws *wsi, unsigned int instance_filter)
{
	unique_lock<mutex> llock(lock);
	
	en_types type = get_type(type_str);
	
	auto it = subscriptions.find(type);
	if(it==subscriptions.end())
		return;
	
	auto it2 = it->second.begin();
	while(it2!=it->second.end())
	{
		if(it2->first==wsi && it2->second.instance_filter==instance_filter)
			it2 = it->second.erase(it2);
		else
			++it;
	}
	
	if(it->second.size()==0)
		subscriptions.erase(type);
}

void Events::UnsubscribeAll(struct lws *wsi)
{
	unique_lock<mutex> llock(lock);
	
	auto it = subscriptions.begin();
	while(it!=subscriptions.end())
	{
		it->second.erase(wsi);
		if(it->second.size()==0)
			it = subscriptions.erase(it);
		else
			++it;
	}
	
	events.erase(wsi);
}

void Events::Create(en_types type, unsigned int instance_id)
{
	unique_lock<mutex> llock(lock);
	
	// Find which client has subscribed to this event
	auto it = subscriptions.find(type);
	if(it==subscriptions.end())
		return;
	
	for(auto it2 = it->second.begin();it2!=it->second.end();++it2)
	{
		// Check instance filter
		if(it2->second.instance_filter!=0 && it2->second.instance_filter!=instance_id)
			continue;
		
		// Push the event to the subscriber
		events[it2->first].insert(it2->second.api_cmd);
		
		lws_callback_on_writable(it2->first);
	}
	
	// Cancel LWS event loop to handle this event
	lws_cancel_service(ws_context);
}

bool Events::Get(struct lws *wsi, string &api_cmd)
{
	unique_lock<mutex> llock(lock);
	
	auto it = events.find(wsi);
	if(it==events.end())
		return false;
	
	auto it2 = it->second.begin();
	if(it2==it->second.end())
		return false;
	
	api_cmd = *it2;
	it->second.erase(api_cmd);
	
	return true;
}
