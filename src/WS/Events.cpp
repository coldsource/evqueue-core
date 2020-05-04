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

using namespace std;

Events *Events::instance = 0;

Events::Events()
{
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
		// Check object filter
		if(it2->second.object_filter!=0 && it2->second.object_filter!=object_id)
			continue;
		
		// Push the event to the subscriber
		events[it2->first].push_back({it2->second.api_cmd, it2->second.external_id, object_id});
		
		Statistics::GetInstance()->IncWSEvents();
		
		lws_callback_on_writable(it2->first);
	}
	
	// Cancel LWS event loop to handle this event
	lws_cancel_service(ws_context);
}

bool Events::Get(struct lws *wsi, int *external_id, unsigned int *object_id, string &api_cmd)
{
	unique_lock<mutex> llock(lock);
	
	auto it = events.find(wsi);
	if(it==events.end())
		return false;
	
	if(it->second.size()==0)
		return false;
	
	api_cmd = it->second.front().api_cmd;
	*external_id = it->second.front().external_id;
	*object_id = it->second.front().object_id;
	it->second.erase(it->second.begin());
	
	return true;
}
