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
#include <Logs/ChannelGroup.h>
#include <Logs/ChannelGroups.h>
#include <Configuration/ConfigurationEvQueue.h>
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
		LogStorage *ls = LogStorage::GetInstance();
		
		unsigned int group_id = query->GetRootAttributeInt("group_id");
		unsigned int limit = query->GetRootAttributeInt("limit",100);
		unsigned int offset = query->GetRootAttributeInt("offset",0);
		
		const ChannelGroup group = ChannelGroups::GetInstance()->Get(group_id);
		
		// Build base select
		string query_select;
		string query_from;
		string query_where;
		string query_order;
		string query_limit;
		vector<void *> values;
		
		auto fields = group.GetFields().GetIDMap();
		query_select = "SELECT c.channel_name, l.log_crit, l.log_date";
		
		query_from = " FROM t_log l ";
		query_from += " INNER JOIN t_channel c ON c.channel_id=l.channel_id ";
		
		for(auto it = fields.begin(); it!=fields.end(); ++it)
		{
			string id_str = to_string(it->first);
			
			query_select += ", v"+to_string(it->first)+".value AS "+it->second.GetName();
			
			query_from += " LEFT JOIN "+it->second.GetTableName()+" v"+id_str;
			query_from += " ON l.log_id=v"+id_str+".log_id AND v"+id_str+".field_id="+id_str+" AND l.log_date=v"+id_str+".log_date ";
		}
		
		
		string filter_crit_str = query->GetRootAttribute("filter_crit","");
		int filter_crit;
		
		string filter_emitted_from = query->GetRootAttribute("filter_emitted_from","");
		string filter_emitted_until = query->GetRootAttribute("filter_emitted_until","");
		
		// Default filter for emitted_from to current day for performance reasons
		if(filter_emitted_from=="")
		{
			char buf[32];
			struct tm now_t;
			time_t now = time(0);
			localtime_r(&now, &now_t);
			strftime(buf, 32, "%Y-%m-%d 00:00:00", &now_t);
			filter_emitted_from = string(buf);
		}
		
		string filter_channel = query->GetRootAttribute("filter_channel","");
		
		DB db("elog");
		
		query_where = " WHERE true ";
		
		if(filter_crit_str!="")
		{
			filter_crit = Field::PackCrit(filter_crit_str);
			query_where += " AND l.log_crit = %i ";
			values.push_back(&filter_crit);
		}
		
		if(filter_emitted_from!="")
		{
			query_where += " AND l.log_date>=%s ";
			values.push_back(&filter_emitted_from);
		}
		
		if(filter_emitted_until!="")
		{
			query_where += " AND l.log_date<=%s ";
			values.push_back(&filter_emitted_until);
		}
		
		if(filter_channel!="")
		{
			query_where += " AND c.channel_name=%s ";
			values.push_back(&filter_channel);
		}
		
		query_order = " ORDER BY l.log_date DESC,l.log_id DESC ";
		
		query_limit = " LIMIT %i,%i ";
		values.push_back(&offset);
		values.push_back(&limit);
		
		db.QueryVsPrintf(query_select+query_from+query_where+query_order+query_limit, values);
		
		while(db.FetchRow())
		{
			DOMElement node = (DOMElement)response->AppendXML("<log />");
			node.setAttribute("channel",db.GetField(0));
			node.setAttribute("crit",Field::UnpackCrit(db.GetFieldInt(1)));
			node.setAttribute("date",db.GetField(2));
			
			int i = 3;
			for(auto it = fields.begin(); it!=fields.end(); ++it)
						node.setAttribute(it->second.GetName(), it->second.Unpack(db.GetField(i++)));
		}
		
		return true;
	}
	else if(action=="statistics")
	{
		DB db;
		
		string dbname = ConfigurationEvQueue::GetInstance()->Get("mysql.database");
		db.QueryPrintf("SELECT PARTITION_NAME, CREATE_TIME, TABLE_ROWS, DATA_LENGTH, INDEX_LENGTH FROM information_schema.partitions WHERE TABLE_SCHEMA=%s AND TABLE_NAME = 't_elog' AND PARTITION_NAME IS NOT NULL ORDER BY PARTITION_DESCRIPTION DESC", &dbname);
		
		while(db.FetchRow())
		{
			DOMElement node = (DOMElement)response->AppendXML("<partition />");
			node.setAttribute("name",db.GetField(0));
			node.setAttribute("creation",db.GetField(1));
			node.setAttribute("rows",db.GetField(2));
			node.setAttribute("datasize",db.GetField(3));
			node.setAttribute("indexsize",db.GetField(4));
		}
		
		return true;
	}
	
	return false;
}
