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

#include <Logs/Logs.h>
#include <Exception/Exception.h>
#include <DB/DB.h>
#include <Logger/Logger.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>

using namespace std;

bool Logs::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	string action = query->GetRootAttribute("action");
	
	if(action=="list")
	{
		unsigned int limit = query->GetRootAttributeInt("limit",100);
		unsigned int offset = query->GetRootAttributeInt("offset",0);
		
		string filter_level = query->GetRootAttribute("filter_level","LOG_DEBUG");
		int ifilter_level = Logger::GetIntegerLogLevel(filter_level);
		
		DB db;
		
		db.QueryPrintf("SELECT node_name,log_level,log_message,log_timestamp FROM t_log WHERE log_level <= %i ORDER BY log_timestamp DESC,log_id DESC LIMIT %i,%i",&ifilter_level,&offset,&limit);
		while(db.FetchRow())
		{
			DOMElement node = (DOMElement)response->AppendXML("<log />");
			node.setAttribute("node",db.GetField(0));
			switch(db.GetFieldInt(1))
			{
				case 0:
					node.setAttribute("level","LOG_EMERG");
					break;
				
				case 1:
					node.setAttribute("level","LOG_ALERT");
					break;
				
				case 2:
					node.setAttribute("level","LOG_CRIT");
					break;
				
				case 3:
					node.setAttribute("level","LOG_ERR");
					break;
				
				case 4:
					node.setAttribute("level","LOG_WARNING");
					break;
				
				case 5:
					node.setAttribute("level","LOG_NOTICE");
					break;
				
				case 6:
					node.setAttribute("level","LOG_INFO");
					break;
				
				case 7:
					node.setAttribute("level","LOG_DEBUG");
					break;
			}
			
			node.setAttribute("message",db.GetField(2));
			node.setAttribute("timestamp",db.GetField(3));
		}
		
		return true;
	}
	
	return false;
}
