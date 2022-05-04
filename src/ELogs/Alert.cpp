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

#include <ELogs/Alert.h>
#include <ELogs/Alerts.h>
#include <ELogs/ChannelGroups.h>
#include <ELogs/ChannelGroup.h>
#include <ELogs/Channels.h>
#include <ELogs/Channel.h>
#include <Notification/Notifications.h>
#include <Notification/Notification.h>
#include <DB/DB.h>
#include <User/User.h>
#include <WS/Events.h>
#include <Logger/LoggerAPI.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <Exception/Exception.h>
#include <API/QueryHandlers.h>
#include <global.h>

using namespace std;
using nlohmann::json;

namespace ELogs
{

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	qh->RegisterHandler("alert", Alert::HandleQuery);
	Events::GetInstance()->RegisterEvents({"ALERT_CREATED", "ALERT_MODIFIED", "ALERT_REMOVED"});
	return (APIAutoInit *)0;
});

Alert::Alert()
{
}

Alert::Alert(DB *db,unsigned int alert_id)
{
	db->QueryPrintf("SELECT alert_id, alert_name, alert_description, alert_occurrences, alert_period, alert_group, alert_filters FROM t_alert WHERE alert_id=%i",&alert_id);
	
	if(!db->FetchRow())
		throw Exception("Alert","Unknown alert");
	
	id = db->GetFieldInt(0);
	name = db->GetField(1);
	description = db->GetField(2);
	occurrences = db->GetFieldInt(3);
	period = db->GetFieldInt(4);
	groupby = db->GetField(5);
	filters = db->GetField(6);
	
	db->QueryPrintf("SELECT notification_id FROM t_alert_notification WHERE alert_id=%i", &alert_id);
	while(db->FetchRow())
		notifications.push_back(db->GetFieldInt(0));
	
	try
	{
		json_filters = json::parse(filters);
	}
	catch(...)
	{
		throw Exception("Alert","Invalid filters json loading alert : "+to_string(alert_id));
	}
}

bool Alert::CheckName(const string &alert_name)
{
	int len;
	
	len = alert_name.length();
	if(len==0 || len>ALERT_NAME_MAX_LEN)
		return false;
	
	return true;
}

void Alert::Get(unsigned int id, QueryResponse *response)
{
	const Alert alert = Alerts::GetInstance()->Get(id);
	
	response->SetAttribute("id",to_string(alert.GetID()));
	response->SetAttribute("name",alert.GetName());
	response->SetAttribute("description",alert.GetDescription());
	response->SetAttribute("occurrences",to_string(alert.GetOccurrences()));
	response->SetAttribute("period",to_string(alert.GetPeriod()));
	response->SetAttribute("groupby",alert.GetGroupby());
	response->SetAttribute("filters",alert.GetFilters());
	
	auto notifications = alert.GetNotifications();
	for(int i=0;i<notifications.size();i++)
	{
		DOMElement node = (DOMElement)response->AppendXML("<notification />");
		node.setAttribute("id",to_string(notifications[i]));
	}
}

unsigned int Alert::Create(const string &name, const string &description, unsigned int occurrences, unsigned int period, const string &groupby, const string &filters, const string &notifications)
{
	 create_edit_check(name, occurrences, period, groupby, filters, notifications);
	
	DB db("elog");
	
	db.StartTransaction();
	
	db.QueryPrintf("INSERT INTO t_alert(alert_name, alert_description, alert_occurrences, alert_period, alert_group, alert_filters) VALUES(%s, %i, %i, %s, %s)",
		&name,
		&description,
		&occurrences,
		&period,
		&groupby,
		&filters
	);
	
	unsigned int id = db.InsertID();
	
	json j_notifications = json::parse(notifications);
	for(auto it = j_notifications.begin(); it!=j_notifications.end(); ++it)
	{
		unsigned int notification_id = stoi(it.key());
		bool issubscribed = it.value();
		
		if(issubscribed)
			db.QueryPrintf("INSERT INTO t_alert_notification(alert_id, notification_id) VALUES(%i,%i)", &id, &notification_id);
	}
	
	db.CommitTransaction();
	
	return id;
}

void Alert::Edit(unsigned int id, const string &name, const string &description, unsigned int occurrences, unsigned int period, const string &groupby, const string &filters, const string &notifications)
{
	create_edit_check(name, occurrences, period, groupby, filters, notifications);
	
	if(!Alerts::GetInstance()->Exists(id))
		throw Exception("Alert","Alert not found","UNKNOWN_ALERT");
	
	DB db("elog");
	
	db.StartTransaction();
	
	db.QueryPrintf("UPDATE t_alert SET alert_name=%s, alert_description=%s, alert_occurrences=%i, alert_period=%i, alert_group=%s, alert_filters=%s WHERE alert_id=%i",
		&name,
		&description,
		&occurrences,
		&period,
		&groupby,
		&filters,
		&id
	);
	
	db.QueryPrintf("DELETE FROM t_alert_notification WHERE alert_id=%i", &id);
	
	json j_notifications = json::parse(notifications);
	for(auto it = j_notifications.begin(); it!=j_notifications.end(); ++it)
	{
		unsigned int notification_id = stoi(it.key());
		bool issubscribed = it.value();
		
		if(issubscribed)
			db.QueryPrintf("INSERT INTO t_alert_notification(alert_id, notification_id) VALUES(%i,%i)", &id, &notification_id);
	}
	
	db.CommitTransaction();
}

void Alert::Delete(unsigned int id)
{
	DB db("elog");
	
	db.StartTransaction();
	
	db.QueryPrintf("DELETE FROM t_alert WHERE alert_id=%i",&id);
	db.QueryPrintf("DELETE FROM t_alert_notification WHERE alert_id=%i");
	
	db.CommitTransaction();
}

void Alert::create_edit_check(const string &name, unsigned int occurrences, unsigned int period, const string &groupby, const string &filters, const string &notifications)
{
	if(!CheckName(name))
		throw Exception("Alert","Invalid alert name","INVALID_PARAMETER");
	
	if(filters=="")
		throw Exception("Alert","Empty fifunctionlters config","INVALID_PARAMETER");
	
	if(notifications=="")
		throw Exception("Alert","Empty notifications config","INVALID_PARAMETER");
	
	if(occurrences==0)
		throw Exception("Alert","Occurrences must be greater than 1","INVALID_PARAMETER");
	
	if(period==0)
		throw Exception("Alert","Period must be greater than 1","INVALID_PARAMETER");
	
	json j_filters;
	
	try
	{
		j_filters = json::parse(filters);
	}
	catch(...)
	{
		throw Exception("Alert","Invalid filters json","INVALID_PARAMETER");
	}
	
	ChannelGroup group;
	Channel channel;
	
	if(j_filters.contains("filter_group"))
	{
		if(j_filters["filter_group"].type()!=nlohmann::json::value_t::number_unsigned)
			throw Exception("Alert","filter_group must be an integer","INVALID_PARAMETER");
		
		unsigned int filter_group = j_filters["filter_group"];
		group = ChannelGroups::GetInstance()->Get(filter_group);
	}
	
	if(j_filters.contains("filter_channel"))
	{
		if(j_filters["filter_channel"].type()!=nlohmann::json::value_t::number_unsigned)
			throw Exception("Alert","filter_channel must be an integer","INVALID_PARAMETER");
		
		unsigned int filter_channel = j_filters["filter_channel"];
		channel = Channels::GetInstance()->Get(filter_channel);
	}
	
	for(auto it = j_filters.begin(); it!=j_filters.end(); ++it)
	{
		string name = it.key();
		if(name.substr(0, 13)=="filter_group_")
		{
			if(!group.GetFields().Exists(name.substr(13)))
				throw Exception("Alert","Unknown group filter : "+name,"INVALID_PARAMETER");
		}
		else if(name.substr(0, 15)=="filter_channel_")
		{
			if(!channel.GetFields().Exists(name.substr(15)))
				throw Exception("Alert","Unknown channel filter : "+name,"INVALID_PARAMETER");
		}
		else if(name!="filter_group" && name!="filter_channel")
			throw Exception("Alert","Unknown filter : "+name,"INVALID_PARAMETER");
	}
	
	if(groupby!="" && groupby!="crit")
	{
		if(groupby.substr(0, 6)=="group_")
		{
			if(!group.GetFields().Exists(groupby.substr(6)))
				throw Exception("Alert","Invalid groupby field : unknown field","INVALID_PARAMETER");
		}
		else if(groupby.substr(0, 8)=="channel_")
		{
			if(!channel.GetFields().Exists(groupby.substr(8)))
				throw Exception("Alert","Invalid groupby field : unknown field","INVALID_PARAMETER");
		}
		else
			throw Exception("Alert","Invalid groupby field","INVALID_PARAMETER");
	}
	
	json j_notifications;
	
	try
	{
		j_notifications = json::parse(notifications);
	}
	catch(...)
	{
		throw Exception("Alert","Invalid notifications json","INVALID_PARAMETER");
	}
	
	for(auto it = j_notifications.begin(); it!=j_notifications.end(); ++it)
	{
		int notification_id;
		try
		{
			notification_id = stoi(it.key());
		}
		catch(...)
		{
			throw Exception("Alert","Invalid notification ID : "+it.key(),"INVALID_PARAMETER");
		}
		
		if(j_notifications[it.key()].type()!=nlohmann::json::value_t::boolean)
			throw Exception("Alert","Notifications must have boolean values","INVALID_PARAMETER");
		
		if(!Notifications::GetInstance()->Exists(notification_id))
			throw Exception("Alert","Unknown notification ID : "+it.key(),"INVALID_PARAMETER");
	}
}

bool Alert::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	string action = query->GetRootAttribute("action");
	
	if(action=="get")
	{
		unsigned int id = query->GetRootAttributeInt("id");
		
		Get(id,response);
		
		return true;
	}
	else if(action=="create" || action=="edit")
	{
		string name = query->GetRootAttribute("name");
		string description = query->GetRootAttribute("description");
		unsigned int occurrences = query->GetRootAttributeInt("occurrences");
		unsigned int period = query->GetRootAttributeInt("period");
		string groupby = query->GetRootAttribute("groupby");
		string filters = query->GetRootAttribute("filters");
		string notifications = query->GetRootAttribute("notifications");
		
		string ev;
		
		if(action=="create")
		{
			unsigned int id = Create(name, description, occurrences, period, groupby, filters, notifications);
			
			LoggerAPI::LogAction(user,id,"Alert",query->GetQueryGroup(),action);
			
			ev = "ALERT_CREATED";
			
			response->GetDOM()->getDocumentElement().setAttribute("alert-id",to_string(id));
		}
		else
		{
			unsigned int id = query->GetRootAttributeInt("id");
			
			Edit(id,name, description, occurrences, period, groupby, filters, notifications);
			
			ev = "ALERT_MODIFIED";
			
			LoggerAPI::LogAction(user,id,"Alert",query->GetQueryGroup(),action);
		}
		
		Alerts::GetInstance()->Reload();
		
		Events::GetInstance()->Create(ev);
		
		return true;
	}
	else if(action=="delete")
	{
		unsigned int id = query->GetRootAttributeInt("id");
		
		Delete(id);
		
		Alerts::GetInstance()->Reload();
		
		LoggerAPI::LogAction(user,id,"Alert",query->GetQueryGroup(),action);
		
		Events::GetInstance()->Create("ALERT_REMOVED", id);
		
		return true;
	}
	
	return false;
}

}
