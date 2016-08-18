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

#include <Logs.h>
#include <Exception.h>
#include <DB.h>
#include <Logger.h>
#include <SocketQuerySAX2Handler.h>
#include <QueryResponse.h>

#include <xqilla/xqilla-dom3.hpp>

using namespace std;
using namespace xercesc;

bool Logs::HandleQuery(SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	string action = saxh->GetRootAttribute("action");
	
	if(action=="list")
	{
		unsigned int limit = saxh->GetRootAttributeInt("limit",100);
		unsigned int offset = saxh->GetRootAttributeInt("offset",0);
		
		DB db;
		
		db.QueryPrintf("SELECT node_name,log_level,log_message,log_timestamp FROM t_log LIMIT %i,%i",&offset,&limit);
		while(db.FetchRow())
		{
			DOMElement *node = (DOMElement *)response->AppendXML("<log />");
			node->setAttribute(X("node"),X(db.GetField(0)));
			node->setAttribute(X("level"),X(db.GetField(1)));
			node->setAttribute(X("message"),X(db.GetField(2)));
			node->setAttribute(X("timestamp"),X(db.GetField(3)));
		}
		
		return true;
	}
	
	return false;
}