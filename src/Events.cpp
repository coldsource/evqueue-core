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

void Events::Subscribe(en_types type, struct lws *wsi)
{
	unique_lock<mutex> llock(lock);
	
	subscribers[type].insert(wsi);
}

void Events::Unsubscribe(en_types type, struct lws *wsi)
{
	unique_lock<mutex> llock(lock);
	
	auto it = subscribers.find(type);
	if(it==subscribers.end())
		return;
	
	it->second.erase(wsi);
	if(it->second.size()==0)
		subscribers.erase(type);
}

void Events::Create(en_types type)
{
	unique_lock<mutex> llock(lock);
	
	events.insert(type);
	
	// Are any client subscribers of this event ?
	{
		auto it = subscribers.find(type);
		if(it==subscribers.end())
			return;
	}
	
	// Notify clients
	{
		auto it = subscribers.find(type);
		if(it==subscribers.end())
			return;
		
		for(auto it2 = it->second.begin();it2!=it->second.end();++it2)
			lws_callback_on_writable(*it2);
	}
	
	// Cancel LWS event loop to handle this event
	lws_cancel_service(ws_context);
}

Events::en_types Events::Get()
{
	unique_lock<mutex> llock(lock);
	
	auto it = events.begin();
	if(it==events.end())
		return NONE;
	
	en_types event = *it;
	events.erase(event);
	return event;
}
