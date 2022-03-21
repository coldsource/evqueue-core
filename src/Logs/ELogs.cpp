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

#include <Logs/ELogs.h>
#include <Logs/LogStorage.h>
#include <Exception/Exception.h>
#include <DB/DB.h>
#include <Logger/Logger.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>

#include <vector>

using namespace std;

bool ELogs::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	string action = query->GetRootAttribute("action");
	
	if(action=="list")
	{
		LogStorage * ls = LogStorage::GetInstance();
		
		unsigned int limit = query->GetRootAttributeInt("limit",100);
		unsigned int offset = query->GetRootAttributeInt("offset",0);
		
		string filter_crit_str = query->GetRootAttribute("filter_crit","");
		int filter_crit;
		
		string filter_emitted_from = query->GetRootAttribute("filter_emitted_from","");
		string filter_emitted_until = query->GetRootAttribute("filter_emitted_until","");
		
		string filter_ip = query->GetRootAttribute("filter_ip","");
		string filter_ip_bin;
		
		string filter_channel = query->GetRootAttribute("filter_channel","");
		
		string filter_uid = query->GetRootAttribute("filter_uid","");
		
		string filter_status_str = query->GetRootAttribute("filter_status","");
		int filter_status;
		if(filter_status_str!="")
		{
			try
			{
				filter_status = stoi(filter_status_str);
			}
			catch(...)
			{
				throw Exception("ELogs","Attribute filter_status must be an integer","INVALID_PARAMETER");
			}
		}
		
		string filter_domain = query->GetRootAttribute("filter_domain","");
		string filter_machine = query->GetRootAttribute("filter_machine","");
		
		DB db;
		
		string query_select;
		string query_from;
		string query_where;
		string query_order;
		string query_limit;
		vector<void *> values;
		
		query_select =" \
			SELECT \
				ec.elog_channel_name, \
				e.elog_date, \
				e.elog_crit, \
				epmachine.elog_pack_string AS elog_machine, \
				epdomain.elog_pack_string AS elog_domain, \
				e.elog_ip, \
				e.elog_uid, \
				e.elog_status, \
				e.elog_fields ";
	
		query_from = " \
			FROM t_elog e \
			INNER JOIN t_elog_channel ec ON e.elog_channel_id=ec.elog_channel_id \
			LEFT JOIN t_elog_pack epmachine ON e.elog_machine=epmachine.elog_pack_id \
			LEFT JOIN t_elog_pack epdomain ON e.elog_domain=epdomain.elog_pack_id ";
		
		query_where = " WHERE true ";
		
		if(filter_crit_str!="")
		{
			filter_crit = ls->PackCrit(filter_crit_str);
			query_where += " AND e.elog_crit = %i ";
			values.push_back(&filter_crit);
		}
		
		if(filter_emitted_from!="")
		{
			query_where += " AND e.elog_date>=%s ";
			values.push_back(&filter_emitted_from);
		}
		
		if(filter_emitted_until!="")
		{
			query_where += " AND e.elog_date<=%s ";
			values.push_back(&filter_emitted_until);
		}
		
		if(filter_ip!="")
		{
			filter_ip_bin = ls->PackIP(filter_ip);
			query_where += " AND e.elog_ip=%s ";
			values.push_back(&filter_ip_bin);
		}
		
		if(filter_channel!="")
		{
			query_where += " AND ec.elog_channel_name=%s ";
			values.push_back(&filter_channel);
		}
		
		if(filter_uid!="")
		{
			query_where += " AND e.elog_uid=%s ";
			values.push_back(&filter_uid);
		}
		
		if(filter_status_str!="")
		{
			query_where += " AND e.elog_status=%i ";
			values.push_back(&filter_status);
		}
		
		if(filter_domain!="")
		{
			query_where += " AND epdomain.elog_pack_string=%s ";
			values.push_back(&filter_domain);
		}
		
		if(filter_machine!="")
		{
			query_where += " AND epmachine.elog_pack_string=%s ";
			values.push_back(&filter_machine);
		}
		
		query_order = " ORDER BY elog_date DESC,elog_id DESC ";
		
		query_limit = " LIMIT %i,%i ";
		values.push_back(&offset);
		values.push_back(&limit);
		
		db.QueryVsPrintf(query_select+query_from+query_where+query_order+query_limit, values);
		
		while(db.FetchRow())
		{
			DOMElement node = (DOMElement)response->AppendXML("<log />");
			node.setAttribute("channel",db.GetField(0));
			node.setAttribute("date",db.GetField(1));
			node.setAttribute("crit",ls->UnpackCrit(db.GetFieldInt(2)));
			node.setAttribute("machine",db.GetField(3));
			node.setAttribute("domain",db.GetField(4));
			if(db.GetField(5)!="")
				node.setAttribute("ip",ls->UnpackIP(db.GetField(5)));
			node.setAttribute("uid",db.GetField(6));
			node.setAttribute("status",db.GetField(7));
			node.setAttribute("custom_fields",db.GetField(8));
		}
		
		return true;
	}
	
	return false;
}
