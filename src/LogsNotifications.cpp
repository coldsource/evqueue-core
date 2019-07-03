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

#include <LogsNotifications.h>
#include <Exception.h>
#include <DB.h>
#include <SocketQuerySAX2Handler.h>
#include <QueryResponse.h>
#include <Workflows.h>
#include <Workflow.h>
#include <WorkflowSchedules.h>
#include <WorkflowSchedule.h>
#include <RetrySchedules.h>
#include <RetrySchedule.h>
#include <Tags.h>
#include <Tag.h>
#include <QueuePool.h>

using namespace std;

bool LogsNotifications::HandleQuery(const User &user, SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	string action = saxh->GetRootAttribute("action");
	
	if(action=="list")
	{
		unsigned int limit = saxh->GetRootAttributeInt("limit",100);
		unsigned int offset = saxh->GetRootAttributeInt("offset",0);
		
		DB db;
		
		db.QueryPrintf(" \
			SELECT node_name,log_notifications_pid,log_notifications_message,log_notifications_timestamp \
			FROM t_log_notifications \
			ORDER BY log_notifications_timestamp DESC,log_notifications_id DESC \
			LIMIT %i,%i",
			&offset,&limit);
		
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
