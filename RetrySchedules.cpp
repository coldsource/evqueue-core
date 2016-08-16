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

#include <string.h>

#include <xqilla/xqilla-dom3.hpp>

RetrySchedules *RetrySchedules::instance = 0;

using namespace std;
using namespace xercesc;

RetrySchedules::RetrySchedules()
{
	instance = this;
	
	pthread_mutex_init(&lock, NULL);
	
	Reload();
}

RetrySchedules::~RetrySchedules()
{
	// Clean current tasks
	map<string,RetrySchedule *>::iterator it;
	for(it=schedules_name.begin();it!=schedules_name.end();++it)
		delete it->second;
	
	schedules_id.clear();
	schedules_name.clear();
}

void RetrySchedules::Reload(void)
{
	Logger::Log(LOG_NOTICE,"[ RetrySchedules ] Reloading schedules definitions");
	
	pthread_mutex_lock(&lock);
	
	// Clean current tasks
	map<string,RetrySchedule *>::iterator it;
	for(it=schedules_name.begin();it!=schedules_name.end();++it)
		delete it->second;
	
	schedules_name.clear();
	schedules_id.clear();
	
	// Update
	DB db;
	DB db2(&db);
	db.Query("SELECT schedule_id,schedule_name FROM t_schedule");
	
	while(db.FetchRow())
	{
		string schedule_name(db.GetField(1));
		RetrySchedule *retry_schedule = new RetrySchedule(&db2,db.GetField(1));
		schedules_id[db.GetFieldInt(0)] = retry_schedule;
		schedules_name[schedule_name] = retry_schedule;
	}
	
	pthread_mutex_unlock(&lock);
}

RetrySchedule RetrySchedules::GetRetrySchedule(unsigned int id)
{
	pthread_mutex_lock(&lock);
	
	auto it = schedules_id.find(id);
	if(it==schedules_id.end())
	{
		pthread_mutex_unlock(&lock);
		
		throw Exception("RetrySchedules","Unable to find schedule");
	}
	
	RetrySchedule schedule = *it->second;
	
	pthread_mutex_unlock(&lock);
	
	return schedule;
}

RetrySchedule RetrySchedules::GetRetrySchedule(const string &name)
{
	pthread_mutex_lock(&lock);
	
	auto it = schedules_name.find(name);
	if(it==schedules_name.end())
	{
		pthread_mutex_unlock(&lock);
		
		throw Exception("RetrySchedules","Unable to find schedule");
	}
	
	RetrySchedule schedule = *it->second;
	
	pthread_mutex_unlock(&lock);
	
	return schedule;
}

bool RetrySchedules::Exists(unsigned int id)
{
	pthread_mutex_lock(&lock);
	
	auto it = schedules_id.find(id);
	if(it==schedules_id.end())
	{
		pthread_mutex_unlock(&lock);
		
		return false;
	}
	
	pthread_mutex_unlock(&lock);
	
	return true;
}

bool RetrySchedules::HandleQuery(SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	RetrySchedules *retry_schedules = RetrySchedules::GetInstance();
	
	const string action = saxh->GetRootAttribute("action");
	
	if(action=="list")
	{
		pthread_mutex_lock(&retry_schedules->lock);
		
		for(auto it = retry_schedules->schedules_name.begin(); it!=retry_schedules->schedules_name.end(); it++)
		{
			RetrySchedule retry_schedule = *it->second;
			DOMElement *node = (DOMElement *)response->AppendXML(retry_schedule.GetXML());
			node->setAttribute(X("id"),X(std::to_string(retry_schedule.GetID()).c_str()));
			node->setAttribute(X("name"),X(retry_schedule.GetName().c_str()));
		}
		
		pthread_mutex_unlock(&retry_schedules->lock);
		
		return true;
	}
	
	return false;
}
