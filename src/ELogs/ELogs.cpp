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
#include <Utils/Date.h>
#include <User/User.h>

#include <vector>

using namespace std;

namespace ELogs
{

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	if(!Configuration::GetInstance()->GetBool("elog.enable"))
		return (APIAutoInit *)0;
	
	qh->RegisterHandler("elogs", ELogs::HandleQuery);
	qh->RegisterModule("elogs", EVQUEUE_VERSION);
	return (APIAutoInit *)0;
});

string ELogs::get_filter(const map<string, string> &filters, const string &name, const string &default_val)
{
	auto it = filters.find(name);
	if(it==filters.end())
		return default_val;
	
	return it->second;
}

int ELogs::get_filter(const map<string, string> &filters, const string &name,int default_val)
{
	auto it = filters.find(name);
	if(it==filters.end())
		return default_val;
	
	try
	{
		return std::stoi(it->second);
	}
	catch(...)
	{
		throw Exception("ELogs","Filter '"+name+"' has invalid integer value","INVALID_INTEGER");
	}
}

vector<map<string, string>> ELogs::QueryLogs(map<string, string> filters, unsigned int limit, unsigned int offset)
{
	ChannelGroup group;
	Channel channel;
	
	// Build base select
	string query_select;
	string query_from;
	string query_where;
	string query_groupby;
	string query_order;
	string query_limit;
	vector<const void *> values;
	
	string groupby;
	if(filters.find("groupby")!=filters.end())
		groupby = filters["groupby"];
	
	ELog::BuildSelectFrom(query_select, query_from, groupby);
	
	// Base filters (always present)
	string filter_crit_str = get_filter(filters, "filter_crit", "");
	int filter_crit;
	
	string filter_emitted_from = get_filter(filters, "filter_emitted_from", "");
	string filter_emitted_until = get_filter(filters, "filter_emitted_until", "");
	
	// Default filter for emitted_from to current day for performance reasons
	if(filter_emitted_from=="")
		filter_emitted_from = Utils::Date::FormatDate("%Y-%m-%d 00:00:00");
	
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
	
	unsigned int filter_group = get_filter(filters, "filter_group", 0);
	if(filter_group!=0)
	{
		query_where += " AND c.channel_group_id=%i ";
		values.push_back(&filter_group);
		
		group = ChannelGroups::GetInstance()->Get(filter_group);
		ELog::BuildSelectFromAppend(query_select, query_from, group.GetFields(), "group_", groupby);
	}
	
	unsigned int filter_channel = get_filter(filters, "filter_channel",0);
	if(filter_channel!=0)
	{
		query_where += " AND l.channel_id=%i ";
		values.push_back(&filter_channel);
		
		channel = Channels::GetInstance()->Get(filter_channel);
		ELog::BuildSelectFromAppend(query_select, query_from, channel.GetFields(), "channel_", groupby);
	}
	
	auto group_fields = group.GetFields().GetIDMap();
	string group_filters_val_str[group_fields.size()];
	int group_filters_val_int[group_fields.size()];
	add_auto_filters(filters, group.GetFields(), "filter_group_", query_where,values,group_filters_val_int, group_filters_val_str);
	
	auto channel_fields = channel.GetFields().GetIDMap();
	string channel_filters_val_str[channel_fields.size()];
	int channel_filters_val_int[channel_fields.size()];
	add_auto_filters(filters, channel.GetFields(), "filter_channel_", query_where,values,channel_filters_val_int, channel_filters_val_str);
	
	if(groupby=="")
		query_order = " ORDER BY l.log_id DESC ";
	else
		query_groupby = " GROUP BY "+groupby;
	
	query_limit = " LIMIT %i OFFSET %i ";
	values.push_back(&limit);
	values.push_back(&offset);
	
	db.QueryPrintf(query_select+query_from+query_where+query_groupby+query_order+query_limit, values);
	
	vector<map<string, string>> results;
	
	while(db.FetchRow())
	{
		map<string, string> result;
		if(groupby=="")
		{
			result["id"] = db.GetField(0);
			result["channel"] = Channels::GetInstance()->Get(db.GetFieldInt(1)).GetName();
			result["crit"] = Field::UnpackCrit(db.GetFieldInt(2));
			result["date"] = db.GetField(3);
			
			int i = 4;
			if(filter_group!=0)
			{
				for(auto it = group_fields.begin(); it!=group_fields.end(); ++it)
					result["group_"+it->second.GetName()] = it->second.Unpack(db.GetField(i++));
			}
			
			if(filter_channel!=0)
			{
				for(auto it = channel_fields.begin(); it!=channel_fields.end(); ++it)
					result["channel_"+it->second.GetName()] = it->second.Unpack(db.GetField(i++));
			}
		}
		else
		{
			result["n"] = db.GetField(0);
			if(groupby=="crit")
				result[groupby] = Field::UnpackCrit(db.GetFieldInt(1));
			else if(groupby.substr(0,6)=="group_")
				result[groupby] = group.GetFields().Get(groupby.substr(6)).Unpack(db.GetField(1));
			else if(groupby.substr(0,8)=="channel_")
				result[groupby] = channel.GetFields().Get(groupby.substr(8)).Unpack(db.GetField(1));
		}
		
		results.push_back(result);
	}
	
	return results;
}

bool ELogs::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	string action = query->GetRootAttribute("action");
	
	if(action=="list")
	{
		unsigned int limit = query->GetRootAttributeInt("limit",100);
		unsigned int offset = query->GetRootAttributeInt("offset",0);
		int filter_group = query->GetRootAttributeInt("filter_group", 0);
		int filter_channel = query->GetRootAttributeInt("filter_channel", 0);
		
		auto res = QueryLogs(query->GetRootAttributes(), limit, offset);
		
		// Add group fields description if filter_group is set
		if(filter_group!=0)
		{
			DOMElement node_group = (DOMElement)response->AppendXML("<group />");
			
			ChannelGroup group = ChannelGroups::GetInstance()->Get(filter_group);
			group.GetFields().AppendXMLDescription(response, node_group);
		}
		
		// Add channel fields description if filter_group is set
		if(filter_channel!=0)
		{
			DOMElement node_channel = (DOMElement)response->AppendXML("<channel />");
			
			Channel channel = Channels::GetInstance()->Get(filter_channel);
			channel.GetFields().AppendXMLDescription(response, node_channel);
		}
		
		DOMElement node_logs = (DOMElement)response->AppendXML("<logs />");
		for(int i=0;i<res.size();i++)
		{
			DOMElement node = (DOMElement)response->AppendXML("<log />", node_logs);
			for(auto it = res[i].begin(); it!=res[i].end(); ++it)
				node.setAttribute(it->first, it->second);
		}
		
		return true;
	}
	else if(action=="statistics")
	{
		DB db("elog");
		
		string dbname = ConfigurationEvQueue::GetInstance()->Get("elog.mysql.database");
		db.QueryPrintf("SELECT PARTITION_NAME, CREATE_TIME, TABLE_ROWS, DATA_LENGTH, INDEX_LENGTH FROM information_schema.partitions WHERE TABLE_SCHEMA=%s AND TABLE_NAME = 't_log' AND PARTITION_NAME IS NOT NULL ORDER BY PARTITION_DESCRIPTION DESC", {&dbname});
		
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

void ELogs::add_auto_filters(const map<string, string> filters, const Fields &fields, const string &prefix, string &query_where, vector<const void *> &values, int *val_int, string *val_str)
{
	int i = 0;
	auto fields_map = fields.GetIDMap();
	
	for(auto it = fields_map.begin(); it!=fields_map.end(); ++it)
	{
		string filter = get_filter(filters, prefix+it->second.GetName(),"");
		if(filter!="")
		{
			if(it->second.GetType()==Field::en_type::ITEXT)
			{
				filter = Sha1String(filter).GetBinary();
				query_where += " AND v"+to_string(it->first)+".value_sha1 = "+it->second.GetDBType()+" ";
				values.push_back(it->second.Pack(filter, &val_int[i], &val_str[i]));
			}
			else if(it->second.GetType()==Field::en_type::CHAR && filter.back()=='*')
			{
				filter = filter.substr(0, filter.size()-1);
				query_where += " AND v"+to_string(it->first)+".value LIKE CONCAT("+it->second.GetDBType()+", '%') ";
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
