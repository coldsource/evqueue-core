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
#include <API/QueryHandlers.h>
#include <User/User.h>
#include <Exception/Exception.h>
#include <DB/DB.h>
#include <Logger/Logger.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <Cluster/Cluster.h>

#include <regex>
#include <map>

namespace ELogs
{

Alerts *Alerts::instance = 0;

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	qh->RegisterHandler("alerts", Alerts::HandleQuery);
	return (APIAutoInit *)new Alerts();
});

using namespace std;

Alerts::Alerts():APIObjectList()
{
	instance = this;
	
	Reload(false);
}

Alerts::~Alerts()
{
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
		Cluster::GetInstance()->Notify("<control action='reload' module='channels' notify='no' />\n");
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
			node.setAttribute("groupby",alert.GetGroupby());
		}
		
		return true;
	}
	
	return false;
}

}
