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

#include <WS/Events.h>
#include <API/Statistics.h>
#include <Logger/Logger.h>
#include <Configuration/ConfigurationEvQueue.h>

using namespace std;

Events *Events::instance = 0;

Events::Events()
{
	throttling = ConfigurationEvQueue::GetInstance()->GetBool("ws.events.throttling");
	this->ws_context = 0;
	instance = this;
}

Events::~Events()
{
	instance = 0;
}

void Events::SetContext(struct lws_context *ws_context)
{
	this->ws_context = ws_context;
}

Events::en_types Events::get_type(const std::string &type_str)
{
	if(type_str=="INSTANCE_STARTED")
		return INSTANCE_STARTED;
	else if(type_str=="INSTANCE_TERMINATED")
		return INSTANCE_TERMINATED;
	else if(type_str=="INSTANCE_REMOVED")
		return INSTANCE_REMOVED;
	else if(type_str=="INSTANCE_TAGGED")
		return INSTANCE_TAGGED;
	else if(type_str=="INSTANCE_UNTAGGED")
		return INSTANCE_UNTAGGED;
	else if(type_str=="TASK_ENQUEUE")
		return TASK_ENQUEUE;
	else if(type_str=="TASK_EXECUTE")
		return TASK_EXECUTE;
	else if(type_str=="TASK_TERMINATE")
		return TASK_TERMINATE;
	else if(type_str=="TASK_PROGRESS")
		return TASK_PROGRESS;
	else if(type_str=="QUEUE_ENQUEUE")
		return QUEUE_ENQUEUE;
	else if(type_str=="QUEUE_DEQUEUE")
		return QUEUE_DEQUEUE;
	else if(type_str=="QUEUE_EXECUTE")
		return QUEUE_EXECUTE;
	else if(type_str=="QUEUE_TERMINATE")
		return QUEUE_TERMINATE;
	else if(type_str=="QUEUE_CREATED")
		return QUEUE_CREATED;
	else if(type_str=="QUEUE_MODIFIED")
		return QUEUE_MODIFIED;
	else if(type_str=="QUEUE_REMOVED")
		return QUEUE_REMOVED;
	else if(type_str=="TAG_CREATED")
		return TAG_CREATED;
	else if(type_str=="TAG_MODIFIED")
		return TAG_MODIFIED;
	else if(type_str=="TAG_REMOVED")
		return TAG_REMOVED;
	else if(type_str=="WORKFLOW_CREATED")
		return WORKFLOW_CREATED;
	else if(type_str=="WORKFLOW_MODIFIED")
		return WORKFLOW_MODIFIED;
	else if(type_str=="WORKFLOW_REMOVED")
		return WORKFLOW_REMOVED;
	else if(type_str=="WORKFLOW_SUBSCRIBED")
		return WORKFLOW_SUBSCRIBED;
	else if(type_str=="WORKFLOW_UNSUBSCRIBED")
		return WORKFLOW_UNSUBSCRIBED;
	else if(type_str=="GIT_PULLED")
		return GIT_PULLED;
	else if(type_str=="GIT_LOADED")
		return GIT_LOADED;
	else if(type_str=="GIT_SAVED")
		return GIT_SAVED;
	else if(type_str=="GIT_REMOVED")
		return GIT_REMOVED;
	else if(type_str=="LOG_ENGINE")
		return LOG_ENGINE;
	else if(type_str=="LOG_NOTIFICATION")
		return LOG_NOTIFICATION;
	else if(type_str=="LOG_API")
		return LOG_API;
	else if(type_str=="LOG_ELOG")
		return LOG_ELOG;
	else if(type_str=="RETRYSCHEDULE_CREATED")
		return RETRYSCHEDULE_CREATED;
	else if(type_str=="RETRYSCHEDULE_MODIFIED")
		return RETRYSCHEDULE_MODIFIED;
	else if(type_str=="RETRYSCHEDULE_REMOVED")
		return RETRYSCHEDULE_REMOVED;
	else if(type_str=="WORKFLOWSCHEDULE_CREATED")
		return WORKFLOWSCHEDULE_CREATED;
	else if(type_str=="WORKFLOWSCHEDULE_MODIFIED")
		return WORKFLOWSCHEDULE_MODIFIED;
	else if(type_str=="WORKFLOWSCHEDULE_REMOVED")
		return WORKFLOWSCHEDULE_REMOVED;
	else if(type_str=="WORKFLOWSCHEDULE_STARTED")
		return WORKFLOWSCHEDULE_STARTED;
	else if(type_str=="WORKFLOWSCHEDULE_STOPPED")
		return WORKFLOWSCHEDULE_STOPPED;
	else if(type_str=="NOTIFICATION_TYPE_CREATED")
		return NOTIFICATION_TYPE_CREATED;
	else if(type_str=="NOTIFICATION_TYPE_REMOVED")
		return NOTIFICATION_TYPE_REMOVED;
	else if(type_str=="NOTIFICATION_CREATED")
		return NOTIFICATION_CREATED;
	else if(type_str=="NOTIFICATION_REMOVED")
		return NOTIFICATION_REMOVED;
	else if(type_str=="NOTIFICATION_MODIFIED")
		return NOTIFICATION_MODIFIED;
	else if(type_str=="USER_CREATED")
		return USER_CREATED;
	else if(type_str=="USER_MODIFIED")
		return USER_MODIFIED;
	else if(type_str=="USER_REMOVED")
		return USER_REMOVED;
	else if(type_str=="CHANNEL_CREATED")
		return CHANNEL_CREATED;
	else if(type_str=="CHANNEL_MODIFIED")
		return CHANNEL_MODIFIED;
	else if(type_str=="CHANNEL_REMOVED")
		return CHANNEL_REMOVED;
	
	return NONE;
}

void Events::Subscribe(const string &type_str, struct lws *wsi, unsigned int object_filter, int external_id, const string &api_cmd)
{
	unique_lock<mutex> llock(lock);
	
	en_types type = get_type(type_str);
	
	subscriptions[type].insert(pair<struct lws *, st_subscription>(wsi,{object_filter, api_cmd, external_id}));
	
	Statistics::GetInstance()->IncWSSubscriptions();
}

void Events::Unsubscribe(const string &type_str, struct lws *wsi, unsigned int object_filter, int external_id)
{
	unique_lock<mutex> llock(lock);
	
	en_types type = get_type(type_str);
	
	auto it = subscriptions.find(type);
	if(it==subscriptions.end())
		return;
	
	auto it2 = it->second.begin();
	while(it2!=it->second.end())
	{
		if(it2->first==wsi && it2->second.object_filter==object_filter && (external_id==0 || external_id==it2->second.external_id))
		{
			it2 = it->second.erase(it2);
			Statistics::GetInstance()->DecWSSubscriptions();
		}
		else
			++it2;
	}
	
	if(it->second.size()==0)
		subscriptions.erase(type);
	
	online_events.erase(wsi);
}

void Events::UnsubscribeAll(struct lws *wsi)
{
	unique_lock<mutex> llock(lock);
	
	auto it = subscriptions.begin();
	while(it!=subscriptions.end())
	{
		int removed = it->second.erase(wsi);
		Statistics::GetInstance()->DecWSSubscriptions(removed);
		
		if(it->second.size()==0)
			it = subscriptions.erase(it);
		else
			++it;
	}
	
	events.erase(wsi);
	online_events.erase(wsi);
}

void Events::insert_event(struct lws *wsi, const st_event &event)
{
	events[wsi].push_back(event);
	
	st_online_event oev;
	oev.event = event;
	
	if(throttling)
		online_events[wsi].push_back(oev);
	
	Statistics::GetInstance()->IncWSEvents();
	
	lws_callback_on_writable(wsi);
}

void Events::Create(const string &type_str, unsigned int object_id)
{
	en_types type = get_type(type_str);
	Create(type, object_id);
}

void Events::Create(en_types type, unsigned int object_id)
{
	if(!this->ws_context)
		return; // Prevent events from being creating before server is ready
	
	unique_lock<mutex> llock(lock);
	
	// Find which client has subscribed to this event
	auto it = subscriptions.find(type);
	if(it==subscriptions.end())
		return;
	
	for(auto it2 = it->second.begin();it2!=it->second.end();++it2)
	{
		struct lws *wsi = it2->first;
		const st_subscription &sub = it2->second;
		
		// Check object filter
		if(sub.object_filter!=0 && sub.object_filter!=object_id)
			continue;
		
		st_event ev = {sub.api_cmd, sub.external_id, to_string(object_id), ++event_id};
		
		if(throttling)
		{
			// Check if this event is online (ie: it has been sent to client but not yet acknowleged)
			auto it_oe = online_events.find(wsi);
			bool skip = false;
			if(it_oe!=online_events.end())
			{
				for(int i=0;i<it_oe->second.size();i++)
				{
					if(it_oe->second[i].event==ev)
					{
						it_oe->second[i].need_resend = true; // Delay event, it will be sent uppon acknowlegement
						if(object_id)
						{
							if(it_oe->second[i].object_ids!="")
								it_oe->second[i].object_ids += ",";
							it_oe->second[i].object_ids += to_string(object_id);
						}
						
						skip = true;
						Logger::Log(LOG_DEBUG, "Throttling: skipped event");
						break;
					}
				}
			}
			
			if(skip)
				continue; // Event has been delayed, we will send it when current event is acknowleged
		}
		
		// Push the event to the subscriber
		insert_event(wsi, ev);
	}
	
	// Cancel LWS event loop to handle this event
	lws_cancel_service(ws_context);
}

bool Events::Get(struct lws *wsi, int *external_id, string &object_id, unsigned long long *event_id, string &api_cmd)
{
	unique_lock<mutex> llock(lock);
	
	auto it = events.find(wsi);
	if(it==events.end())
		return false;
	
	if(it->second.size()==0)
		return false;
	
	const st_event &ev = it->second.front();
	
	api_cmd = ev.api_cmd;
	*external_id = ev.external_id;
	object_id = ev.object_id;
	*event_id = ev.event_id;
	it->second.erase(it->second.begin());
	
	return true;
}

void Events::Ack(struct lws *wsi, unsigned long long ack_event_id)
{
	if(!throttling)
		return;
	
	unique_lock<mutex> llock(lock);
	
	auto it = online_events.find(wsi);
	if(it==online_events.end())
		return;
	
	for(int i=0;i<it->second.size();i++)
	{
		if(it->second[i].event.event_id==ack_event_id)
		{
			st_online_event oev = it->second[i];
			
			// Event is no more online
			it->second.erase(it->second.begin()+i);
			
			if(!oev.need_resend)
				return;
			
			// An event of this type has been delayed, we need to send delayed event now
			st_event ev = oev.event;
			ev.event_id = ++event_id;
			ev.object_id = oev.object_ids;
			
			insert_event(wsi, ev);
			
			lws_cancel_service(ws_context);
			return;
		}
	}
}
