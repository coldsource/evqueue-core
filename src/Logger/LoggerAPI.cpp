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

#include <Logger/LoggerAPI.h>
#include <DB/DB.h>
#include <User/User.h>
#include <Configuration/ConfigurationEvQueue.h>
#include <WS/Events.h>

using namespace std;

LoggerAPI *LoggerAPI::instance = 0;

LoggerAPI::LoggerAPI():
	node_name(ConfigurationEvQueue::GetInstance()->Get("cluster.node.name"))
{
	instance = this;
	
	enabled = ConfigurationEvQueue::GetInstance()->GetBool("loggerapi.enable");
}

void LoggerAPI::LogAction(const User &user, unsigned int object_id, const string &object_type, const string &group, const string &action)
{
	if(!instance->enabled)
		return;
	
	DB db;
	unsigned int user_id = user.GetID();
	
	db.QueryPrintf("INSERT INTO t_log_api(node_name,user_id,log_api_object_id,log_api_object_type,log_api_group,log_api_action) VALUES(%s,%i,%i,%s,%s,%s)",
		&instance->node_name,
		&user_id,
		&object_id,
		&object_type,
		&group,
		&action
		);
	
	Events::GetInstance()->Create("LOG_API");
}
