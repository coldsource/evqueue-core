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

#include <Logs/LogsNotifications.h>
#include <Exception/Exception.h>
#include <DB/DB.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <Queue/QueuePool.h>
#include <API/QueryHandlers.h>
#include <WS/Events.h>

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	qh->RegisterHandler("logsnotifications", LogsNotifications::HandleQuery);
	Events::GetInstance()->RegisterEvent("LOG_NOTIFICATION");
	return (APIAutoInit *)0;
});

using namespace std;

bool LogsNotifications::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	string action = query->GetRootAttribute("action");
	
	if(action=="list")
	{
		unsigned int limit = query->GetRootAttributeInt("limit",100);
		unsigned int offset = query->GetRootAttributeInt("offset",0);
		
		DB db;
		
		db.QueryPrintf(" \
			SELECT node_name,log_notifications_pid,log_notifications_message,log_notifications_timestamp \
			FROM t_log_notifications \
			ORDER BY log_notifications_timestamp DESC,log_notifications_id DESC \
			LIMIT %i,%i",
			{&offset,&limit});
		
		while(db.FetchRow())
		{
			DOMElement node = (DOMElement)response->AppendXML("<log />");
			node.setAttribute("node",db.GetField(0));
			node.setAttribute("pid",db.GetField(1));
			node.setAttribute("message",db.GetField(2));
			node.setAttribute("timestamp",db.GetField(3));
		}
		
		return true;
	}
	
	return false;
}
