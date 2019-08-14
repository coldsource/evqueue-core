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

#include <Logger/LoggerNotifications.h>
#include <DB/DB.h>
#include <User/User.h>
#include <Configuration/ConfigurationEvQueue.h>

using namespace std;

LoggerNotifications *LoggerNotifications::instance = 0;

LoggerNotifications::LoggerNotifications():
	node_name(ConfigurationEvQueue::GetInstance()->Get("cluster.node.name"))
{
	instance = this;
}

void LoggerNotifications::Log(pid_t pid, const string &log)
{
	DB db;
	
	db.QueryPrintf("INSERT INTO t_log_notifications(node_name,log_notifications_pid,log_notifications_message) VALUES(%s,%i,%s)",
		&instance->node_name,
		&pid,
		&log
		);
}
