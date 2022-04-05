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
#include <Configuration/ConfigurationEvQueue.h>
#include <Exception/Exception.h>
#include <DB/DB.h>
#include <Logger/Logger.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <Crypto/Sha1String.h>

#include <vector>

using namespace std;

namespace ELogs
{

bool ELogs::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	string action = query->GetRootAttribute("action");
	
	if(action=="list")
	{
		int i;
		
		LogStorage *ls = LogStorage::GetInstance();
		unsigned int group_id = query->GetRootAttributeInt("group_id");
		unsigned int limit = query->GetRootAttributeInt("limit",100);
		unsigned int offset = query->GetRootAttributeInt("offset",0);
		
		const ChannelGroup group = ChannelGroups::GetInstance()->Get(group_id);
		Channel channel;
		
		// Build base select
		string query_select;
		string query_from;
		string query_where;
		string query_order;
		string query_limit;
		vector<void *> values;
		
		ELog::BuildSelectFrom(query_select, query_from, group.GetFields());
		
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
		
		query_where = " WHERE c.channel_group_id=%i ";
		values.push_back(&group_id);
		
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
		
		unsigned int filter_channel = query->GetRootAttributeInt("filter_channel",0);
		if(filter_channel!=0)
		{
			query_where += " AND l.channel_id=%i ";
			values.push_back(&filter_channel);
			
			channel = Channels::GetInstance()->Get(filter_channel);
			ELog::BuildSelectFromAppend(query_select, query_from, channel.GetFields());
		}
		
		auto group_fields = group.GetFields().GetIDMap();
		string group_filters[group_fields.size()];
		string group_filters_val_str[group_fields.size()];
		int group_filters_val_int[group_fields.size()];
		i = 0;
		for(auto it = group_fields.begin(); it!=group_fields.end(); ++it)
		{
			group_filters[i] = query->GetRootAttribute("filter_group_"+it->second.GetName(),"");
			if(group_filters[i]!="")
			{
				if(it->second.GetType()==Field::en_type::ITEXT)
				{
					group_filters[i] = Sha1String(group_filters[i]).GetBinary();
					query_where += " AND v"+to_string(it->first)+".value_sha1 = "+it->second.GetDBType()+" ";
					values.push_back(it->second.Pack(group_filters[i], &group_filters_val_int[i], &group_filters_val_str[i]));
				}
				else
				{
					query_where += " AND v"+to_string(it->first)+".value = "+it->second.GetDBType()+" ";
					values.push_back(it->second.Pack(group_filters[i], &group_filters_val_int[i], &group_filters_val_str[i]));
				}
			}
			
			i++;
		}
		
		auto channel_fields = channel.GetFields().GetIDMap();
		string channel_filters[channel_fields.size()];
		string channel_filters_val_str[channel_fields.size()];
		int channel_filters_val_int[channel_fields.size()];
		i = 0;
		for(auto it = channel_fields.begin(); it!=channel_fields.end(); ++it)
		{
			channel_filters[i] = query->GetRootAttribute("filter_channel_"+it->second.GetName(),"");
			if(channel_filters[i]!="")
			{
				if(it->second.GetType()==Field::en_type::ITEXT)
				{
					channel_filters[i] = Sha1String(channel_filters[i]).GetBinary();
					query_where += " AND v"+to_string(it->first)+".value_sha1 = "+it->second.GetDBType()+" ";
					values.push_back(it->second.Pack(channel_filters[i], &channel_filters_val_int[i], &channel_filters_val_str[i]));
				}
				else
				{
					query_where += " AND v"+to_string(it->first)+".value = "+it->second.GetDBType()+" ";
					values.push_back(it->second.Pack(channel_filters[i], &channel_filters_val_int[i], &channel_filters_val_str[i]));
				}
			}
			
			i++;
		}
		
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

}
