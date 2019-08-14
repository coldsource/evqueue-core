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

#include <RetrySchedules.h>
#include <RetrySchedule.h>
#include <Exception.h>
#include <SocketQuerySAX2Handler.h>
#include <QueryResponse.h>
#include <Logger.h>
#include <DB.h>
#include <Cluster.h>
#include <User.h>

#include <string.h>

RetrySchedules *RetrySchedules::instance = 0;

using namespace std;

RetrySchedules::RetrySchedules():APIObjectList()
{
	instance = this;
	
	Reload(false);
}

RetrySchedules::~RetrySchedules()
{
}

void RetrySchedules::Reload(bool notify)
{
	Logger::Log(LOG_NOTICE,"Reloading schedules definitions");
	
	unique_lock<mutex> llock(lock);
	
	// Clean current tasks
	clear();
	
	// Update
	DB db;
	DB db2(&db);
	db.Query("SELECT schedule_id,schedule_name FROM t_schedule");
	
	while(db.FetchRow())
		add(db.GetFieldInt(0),db.GetField(1),new RetrySchedule(&db2,db.GetField(1)));
	
	llock.unlock();
	
	if(notify)
	{
		// Notify cluster
		Cluster::GetInstance()->Notify("<control action='reload' module='retry_schedules' notify='no' />\n");
	}
}

bool RetrySchedules::HandleQuery(const User &user, SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	RetrySchedules *retry_schedules = RetrySchedules::GetInstance();
	
	const string action = saxh->GetRootAttribute("action");
	
	if(action=="list")
	{
		unique_lock<mutex> llock(retry_schedules->lock);
		
		for(auto it = retry_schedules->objects_name.begin(); it!=retry_schedules->objects_name.end(); it++)
		{
			RetrySchedule retry_schedule = *it->second;
			DOMElement node = (DOMElement)response->AppendXML(retry_schedule.GetXML());
			node.setAttribute("id",to_string(retry_schedule.GetID()));
			node.setAttribute("name",retry_schedule.GetName());
		}
		
		return true;
	}
	
	return false;
}
