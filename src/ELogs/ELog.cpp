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
#include <API/QueryHandlers.h>
#include <User/User.h>

#include <vector>

using namespace std;

namespace ELogs
{

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	qh->RegisterHandler("elog", ELog::HandleQuery);
	return (APIAutoInit *)0;
});

bool ELog::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	string action = query->GetRootAttribute("action");
	
	if(action=="get")
	{
		unsigned long long id = query->GetRootAttributeLong("id");
		
		DB db("elog");
		db.QueryPrintf("SELECT l.channel_id, c.channel_group_id FROM t_log l INNER JOIN t_channel c ON l.channel_id=c.channel_id WHERE l.log_id=%l", {&id});
		db.FetchRow();
		
		unsigned int channel_id = db.GetFieldInt(0);
		unsigned int group_id = db.GetFieldInt(1);
		
		const Channel channel = Channels::GetInstance()->Get(channel_id);
		const ChannelGroup group = ChannelGroups::GetInstance()->Get(group_id);
		
		DOMElement group_node = (DOMElement)response->AppendXML("<group />");
		query_fields(&db, id, group.GetFields(), group_node);
		
		DOMElement channel_node = (DOMElement)response->AppendXML("<channel />");
		query_fields(&db, id, channel.GetFields(), channel_node);
		
		return true;
	}
	else if(action=="statistics")
	{
		DB db;
		
		string dbname = ConfigurationEvQueue::GetInstance()->Get("mysql.database");
		db.QueryPrintf("SELECT PARTITION_NAME, CREATE_TIME, TABLE_ROWS, DATA_LENGTH, INDEX_LENGTH FROM information_schema.partitions WHERE TABLE_SCHEMA=%s AND TABLE_NAME = 't_elog' AND PARTITION_NAME IS NOT NULL ORDER BY PARTITION_DESCRIPTION DESC", {&dbname});
		
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

void ELog::query_fields(DB *db, unsigned long long id, const Fields &fields, DOMElement &node)
{
	string query_select;
	string query_from;
	string query_where;
	
	BuildSelectFrom(query_select, query_from);
	BuildSelectFromAppend(query_select, query_from, fields);
	
	query_where = " WHERE l.log_id=%l ";
	
	db->QueryPrintf(query_select+query_from+query_where, {&id});
	if(!db->FetchRow())
		return;
	
	int i = 4;
	auto fields_map = fields.GetIDMap();
	for(auto it = fields_map.begin(); it!=fields_map.end(); ++it)
		node.setAttribute(it->second.GetName(), it->second.Unpack(db->GetField(i++)));
}

void ELog::BuildSelectFrom(string &query_select, string &query_from, const string &groupby)
{
	if(groupby=="")
		query_select = "SELECT l.log_id, c.channel_id, l.log_crit, l.log_date";
	else if(groupby=="crit")
		query_select = "SELECT COUNT(*) AS n, l.log_crit AS crit";
	else
		query_select = "SELECT COUNT(*) AS n";
	
	query_from = " FROM t_log l ";
	query_from += " INNER JOIN t_channel c ON c.channel_id=l.channel_id ";
}

void ELog::BuildSelectFromAppend(string &query_select, string &query_from, const Fields &fields, const string &prefix, const string &groupby)
{
	auto fields_map = fields.GetIDMap();
	
	for(auto it = fields_map.begin(); it!=fields_map.end(); ++it)
	{
		string id_str = to_string(it->first);
		
		if(groupby=="" || prefix+it->second.GetName()==groupby)
			query_select += ", v"+to_string(it->first)+".value AS "+prefix+it->second.GetName();
		
		query_from += " LEFT JOIN "+it->second.GetTableName()+" v"+id_str;
		query_from += " ON l.log_id=v"+id_str+".log_id AND v"+id_str+".field_id="+id_str+" AND l.log_date=v"+id_str+".log_date ";
	}
}

}
