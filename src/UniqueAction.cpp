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

#include <UniqueAction.h>
#include <DB.h>
#include <ConfigurationEvQueue.h>
#include <Logger.h>
#include <Exception.h>
#include <Cluster.h>

#include <vector>
#include <algorithm>

using namespace std;

UniqueAction::UniqueAction(const string &name, int period)
{
	DB db;
	
	db.QueryPrintf("INSERT INTO t_uniqueaction(node_name,uniqueaction_name) VALUES(%s,%s)",&ConfigurationEvQueue::GetInstance()->Get("cluster.node.name"),&name);
	unsigned int myid = db.InsertID();
	
	if(period>0)
		db.QueryPrintf("SELECT uniqueaction_id FROM t_uniqueaction WHERE uniqueaction_name=%s AND DATE_ADD(uniqueaction_time,INTERVAL %i SECOND)>NOW()",&name,&period);
	else
		db.QueryPrintf("SELECT uniqueaction_id FROM t_uniqueaction WHERE uniqueaction_name=%s",&name);
	
	is_elected = true;
	while(db.FetchRow())
	{
		if(db.GetFieldInt(0)<myid)
		{
			is_elected = false;
			break;
		}
	}
	
	if(!is_elected)
	{
		db.QueryPrintf("DELETE FROM t_uniqueaction WHERE uniqueaction_id=%i",&myid);
		
		Logger::Log(LOG_INFO,"Not elected for cluster unique action '"+name+"'");
	}
	else
		Logger::Log(LOG_NOTICE,"Node "+ConfigurationEvQueue::GetInstance()->Get("cluster.node.name")+" elected for action '"+name+"'");
}

UniqueAction::UniqueAction(const string &name)
{
	vector<string> nodes = Cluster::GetInstance()->Ping();
	
	if(nodes.size()==0)
	{
		// Cluster is not configured, nothing to do
		is_elected = true;
		return;
	}
	
	auto min = min_element(nodes.begin(),nodes.end());
	is_elected = (*min==ConfigurationEvQueue::GetInstance()->Get("cluster.node.name"));
	
	if(!is_elected)
		Logger::Log(LOG_INFO,"Not elected for cluster unique action '"+name+"'");
	else
		Logger::Log(LOG_NOTICE,"Node "+ConfigurationEvQueue::GetInstance()->Get("cluster.node.name")+" elected for action '"+name+"'");
}
