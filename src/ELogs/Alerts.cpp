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

#include <ELogs/Alerts.h>
#include <Configuration/Configuration.h>
#include <API/QueryHandlers.h>
#include <User/User.h>
#include <Exception/Exception.h>
#include <DB/DB.h>
#include <Logger/Logger.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <Cluster/Cluster.h>
#include <ELogs/ELogs.h>
#include <Notification/Notifications.h>
#include <Notification/NotificationTypes.h>
#include <WS/Events.h>
#include <Utils/Date.h>
#include <Cluster/UniqueAction.h>

#include <regex>
#include <map>

namespace ELogs
{

Alerts *Alerts::instance = 0;

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	if(!Configuration::GetInstance()->GetBool("elog.enable"))
		return (APIAutoInit *)0;
	
	qh->RegisterHandler("alerts", Alerts::HandleQuery);
	qh->RegisterReloadHandler("alerts", Alerts::HandleReload);
	Events::GetInstance()->RegisterEvents({"ALERT_TRIGGER"});
	return (APIAutoInit *)new Alerts();
});

using namespace std;
using nlohmann::json;

Alerts::Alerts():APIObjectList("Alert")
{
	instance = this;
	
	Reload(false);
}

Alerts::~Alerts()
{
	Shutdown();
}

void Alerts::APIReady()
{
	NotificationTypes::GetInstance()->RegisterDeleteHandler("ELOGS", HandleNotificationTypeDelete);
	
	start();
}

void Alerts::Reload(bool notify)
{
	Logger::Log(LOG_NOTICE,"Reloading alerts definitions");
	
	unique_lock<mutex> llock(lock);
	
	clear();
	
	// Update
	DB db("elog");
	DB db2(&db);
	db.Query("SELECT alert_id, alert_name FROM t_alert");
	
	while(db.FetchRow())
		add(db.GetFieldInt(0),db.GetField(1),new Alert(&db2,db.GetFieldInt(0)));
	
	llock.unlock();
	
	if(notify)
	{
		// Notify cluster
		Cluster::GetInstance()->Notify("<control action='reload' module='alerts' notify='no' />\n");
	}
}

bool Alerts::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	Alerts *alerts = Alerts::GetInstance();
	
	string action = query->GetRootAttribute("action");
	
	if(action=="list")
	{
		unique_lock<mutex> llock(alerts->lock);
		
		for(auto it = alerts->objects_name.begin(); it!=alerts->objects_name.end(); it++)
		{
			Alert alert = *it->second;
			
			DOMElement node = (DOMElement)response->AppendXML("<alert />");
			node.setAttribute("id",to_string(alert.GetID()));
			node.setAttribute("name",alert.GetName());
			node.setAttribute("description",alert.GetDescription());
			node.setAttribute("occurrences", to_string(alert.GetOccurrences()));
			node.setAttribute("period", to_string(alert.GetPeriod()));
			node.setAttribute("active",alert.GetIsActive()?"1":"0");
			node.setAttribute("groupby",alert.GetGroupby());
		}
		
		return true;
	}
	
	return false;
}

void Alerts::HandleReload(bool notify)
{
	Alerts::GetInstance()->Reload(notify);
}

void Alerts::HandleNotificationTypeDelete(unsigned int id)
{
	DB db;
	DB dbelog("elog");
	
	db.QueryPrintf("SELECT notification_id FROM t_notification WHERE notification_type_id=%i", {&id});
	while(db.FetchRow())
	{
		int notification_id = db.GetFieldInt(0);
		
		// Ensure no elogs alerts are bound to removed notifications
		dbelog.QueryPrintf("DELETE FROM t_alert_notification WHERE notification_id=%i", {&notification_id});
	}
	
	Alerts::GetInstance()->Reload();
	
	Events::GetInstance()->Create("ALERT_MODIFIED");
}

void Alerts::main()
{
	unsigned int timer = 0;
	
	DB::StartThread();
	
	while(1)
	{
		// Our time resolution is 1 minute, so wait 60 seconds
		if(!wait(60))
		{
			Logger::Log(LOG_NOTICE,"Shutdown in progress exiting Alerts Engine");
			
			DB::StopThread();
			
			return;
		}
		
		Logger::Log(LOG_INFO, "Processing alerts...");
		
		timer++;
		
		// Fetch all alerts
		vector<Alert> alert_objs;
		{
			unique_lock<mutex> llock(lock);
			
			for(auto it = objects_name.begin(); it!=objects_name.end(); it++)
				alert_objs.push_back(*it->second);
		}
		
		for(int i=0;i<alert_objs.size();i++)
		{
			Alert alert = alert_objs[i];
			
			if(!alert.GetIsActive())
				continue; // Alert has been disactiated
			
			if(timer%alert.GetPeriod()!=0)
				continue; // Skip alert if period is not yet reached
			
			UniqueAction uaction("elogs_alerts_" + alert.GetName(),alert.GetPeriod() * 60);
			if(!uaction.IsElected())
				continue;
			
			Logger::Log(LOG_INFO, "Processing alert «" + alert.GetName() + "»");
			
			try
			{
				// Convert json filters to map
				auto json_filters = alert.GetJsonFilters();
				map<string, string> filters;
				for(auto it = json_filters.begin(); it!=json_filters.end(); ++it)
				{
					if(it.value().type()==nlohmann::json::value_t::number_unsigned)
						filters[it.key()] = to_string((int)it.value());
					else if(it.value().type()==nlohmann::json::value_t::string)
						filters[it.key()] = it.value();
				}
				
				// Add start date based on alert period
				string start_date = Utils::Date::PastDate(alert.GetPeriod() * 60);
				filters["filter_emitted_from"] = start_date;
				
				bool is_groupped = alert.GetIsGroupped();
				
				auto logs = ELogs::QueryLogs(filters, 1000);
				if(!is_groupped && logs.size()<alert.GetOccurrences())
					continue; // Too few logs to trigger
				
				// Build json data for notification script
				json j_logs = json::array();
				for(int i=0;i<logs.size(); i++)
				{
					json j_log;
					
					try
					{
						if(is_groupped && stoi(logs[i]["n"])<alert.GetOccurrences())
							continue; // To few groupped logs
					}
					catch(...)
					{
						continue; // Should not happen
					}
					
					for(auto field = logs[i].begin(); field!=logs[i].end(); ++field)
						j_log[field->first] = field->second;
					j_logs.push_back(j_log);
				}
				
				if(j_logs.size()==0)
					continue; // No logs (everything was filtered in the loop), so do not call alert
				
				json j;
				j["logs"] = j_logs;
				
				if(is_groupped)
					j["groupby"] = filters["groupby"];
				
				auto notifications = alert.GetNotifications();
				for(int i=0;i<notifications.size(); i++)
				{
					Notifications::GetInstance()->Call(
						notifications.at(i),
						"alert "+alert.GetName(),
						{},
						j
					);
				}
				
				// Log alert trigger
				DB db("elog");
				int alert_id = alert.GetID();
				db.QueryPrintf(
					"INSERT INTO t_alert_trigger(alert_id, alert_trigger_start, alert_trigger_filters) VALUES(%i,%s, %s)", {
					&alert_id,
					&start_date,
					&alert.GetFilters()
				});
				
				// Emit event
				Events::GetInstance()->Create("ALERT_TRIGGER");
			}
			catch(Exception &e)
			{
				Logger::Log(LOG_ERR,"Alerts Engine : error processing alert \""+alert.GetName()+"\" : "+e.error);
			}
		}
	}
	
	return;
}

}
