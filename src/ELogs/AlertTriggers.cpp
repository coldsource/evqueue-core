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

#include <ELogs/AlertTriggers.h>
#include <Exception/Exception.h>
#include <DB/DB.h>
#include <Logger/Logger.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <API/QueryHandlers.h>
#include <WS/Events.h>

namespace ELogs
{

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	qh->RegisterHandler("alert_triggers", AlertTriggers::HandleQuery);
	return (APIAutoInit *)0;
});

using namespace std;

bool AlertTriggers::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	string action = query->GetRootAttribute("action");
	
	if(action=="list")
	{
		unsigned int limit = query->GetRootAttributeInt("limit",100);
		unsigned int offset = query->GetRootAttributeInt("offset",0);
		
		DB db("elog");
		
		db.QueryPrintf(" \
			SELECT at.alert_trigger_id, a.alert_name, at.alert_trigger_start, at.alert_trigger_date, at.alert_trigger_filters \
			FROM t_alert_trigger at \
			INNER JOIN t_alert a ON a.alert_id=at.alert_id \
			ORDER BY at.alert_trigger_date DESC \
			LIMIT %i OFFSET %i", {
			&limit,
			&offset
		});
		
		while(db.FetchRow())
		{
			DOMElement node = (DOMElement)response->AppendXML("<trigger />");
			node.setAttribute("id", db.GetField(0));
			node.setAttribute("alert_name", db.GetField(1));
			node.setAttribute("start", db.GetField(2));
			node.setAttribute("date", db.GetField(3));
			node.setAttribute("filters", db.GetField(4));
		}
		
		return true;
	}
	else if(action=="delete")
	{
		DB db("elog");
		
		unsigned int id = query->GetRootAttributeInt("id");
		
		db.QueryPrintf("DELETE FROM t_alert_trigger WHERE alert_trigger_id=%i", {&id});
		
		Events::GetInstance()->Create("ALERT_TRIGGER");
		
		return true;
	}
		
	
	return false;
}

}
