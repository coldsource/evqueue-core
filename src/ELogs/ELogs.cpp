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

#include <ELogs/ELogs.h>
#include <ELogs/ELog.h>
#include <ELogs/LogStorage.h>
#include <ELogs/ChannelGroup.h>
#include <ELogs/ChannelGroups.h>
#include <ELogs/Channel.h>
#include <ELogs/Channels.h>
#include <ELogs/Fields.h>
#include <Configuration/ConfigurationEvQueue.h>
#include <Exception/Exception.h>
#include <DB/DB.h>
#include <Logger/Logger.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <Crypto/Sha1String.h>
#include <API/QueryHandlers.h>

#include <vector>

using namespace std;

namespace ELogs
{

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	qh->RegisterHandler("elogs", ELogs::HandleQuery);
	return (APIAutoInit *)0;
});

bool ELogs::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	string action = query->GetRootAttribute("action");
	
	if(action=="list")
	{
		unsigned int limit = query->GetRootAttributeInt("limit",100);
		unsigned int offset = query->GetRootAttributeInt("offset",0);
		
		ChannelGroup group;
		Channel channel;
		
		// Build base select
		string query_select;
		string query_from;
		string query_where;
		string query_order;
		string query_limit;
		vector<void *> values;
		
		ELog::BuildSelectFrom(query_select, query_from);
		
		// Base filters (always present)
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
		
		unsigned int filter_group = query->GetRootAttributeInt("filter_group", 0);
		if(filter_group!=0)
		{
			query_where += " AND c.channel_group_id=%i ";
			values.push_back(&filter_group);
			
			group = ChannelGroups::GetInstance()->Get(filter_group);
			ELog::BuildSelectFromAppend(query_select, query_from, group.GetFields());
		}
		
		unsigned int filter_channel = query->GetRootAttributeInt("filter_channel",0);
		if(filter_channel!=0)
		{
			query_where += " AND l.channel_id=%i ";
			values.push_back(&filter_channel);
			
			channel = Channels::GetInstance()->Get(filter_channel);
			ELog::BuildSelectFromAppend(query_select, query_from, channel.GetFields());
		}
		
		auto group_fields = group.GetFields().GetIDMap();
		string group_filters_val_str[group_fields.size()];
		int group_filters_val_int[group_fields.size()];
		add_auto_filters(query, group.GetFields(), "filter_group_", query_where,values,group_filters_val_int, group_filters_val_str);
		
		auto channel_fields = channel.GetFields().GetIDMap();
		string channel_filters_val_str[channel_fields.size()];
		int channel_filters_val_int[channel_fields.size()];
		add_auto_filters(query, channel.GetFields(), "filter_channel_", query_where,values,channel_filters_val_int, channel_filters_val_str);
		
		query_order = " ORDER BY l.log_id DESC ";
		
		query_limit = " LIMIT %i,%i ";
		values.push_back(&offset);
		values.push_back(&limit);
		
		db.QueryVsPrintf(query_select+query_from+query_where+query_order+query_limit, values);
		
		while(db.FetchRow())
		{
			DOMElement node = (DOMElement)response->AppendXML("<log />");
			node.setAttribute("id",db.GetField(0));
			node.setAttribute("channel",db.GetField(1));
			node.setAttribute("crit",Field::UnpackCrit(db.GetFieldInt(2)));
			node.setAttribute("date",db.GetField(3));
			
			int i = 4;
			for(auto it = group_fields.begin(); it!=group_fields.end(); ++it)
				node.setAttribute(it->second.GetName(), it->second.Unpack(db.GetField(i++)));
			
			if(filter_channel!=0)
			{
				for(auto it = channel_fields.begin(); it!=channel_fields.end(); ++it)
				node.setAttribute("channel_"+it->second.GetName(), it->second.Unpack(db.GetField(i++)));
			}
		}
		
		return true;
	}
	else if(action=="statistics")
	{
		DB db("elog");
		
		string dbname = ConfigurationEvQueue::GetInstance()->Get("elog.mysql.database");
		db.QueryPrintf("SELECT PARTITION_NAME, CREATE_TIME, TABLE_ROWS, DATA_LENGTH, INDEX_LENGTH FROM information_schema.partitions WHERE TABLE_SCHEMA=%s AND TABLE_NAME = 't_log' AND PARTITION_NAME IS NOT NULL ORDER BY PARTITION_DESCRIPTION DESC", &dbname);
		
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

void ELogs::add_auto_filters(XMLQuery *query, const Fields &fields, const string &prefix, string &query_where, vector<void *> &values, int *val_int, string *val_str)
{
	int i = 0;
	auto fields_map = fields.GetIDMap();
	
	for(auto it = fields_map.begin(); it!=fields_map.end(); ++it)
	{
		string filter = query->GetRootAttribute(prefix+it->second.GetName(),"");
		if(filter!="")
		{
			if(it->second.GetType()==Field::en_type::ITEXT)
			{
				filter = Sha1String(filter).GetBinary();
				query_where += " AND v"+to_string(it->first)+".value_sha1 = "+it->second.GetDBType()+" ";
				values.push_back(it->second.Pack(filter, &val_int[i], &val_str[i]));
			}
			else
			{
				query_where += " AND v"+to_string(it->first)+".value = "+it->second.GetDBType()+" ";
				values.push_back(it->second.Pack(filter, &val_int[i], &val_str[i]));
			}
		}
		
		i++;
	}
}

}
