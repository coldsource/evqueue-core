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
#include <Logger.h>
#include <DB.h>

#include <string.h>

RetrySchedules *RetrySchedules::instance = 0;

RetrySchedules::RetrySchedules()
{
	instance = this;
	
	pthread_mutex_init(&lock, NULL);
	
	Reload();
}

void RetrySchedules::Reload(void)
{
	Logger::Log(LOG_NOTICE,"[ RetrySchedules ] Reloading schedules definitions");
	
	pthread_mutex_lock(&lock);
	
	// Clean current tasks
	std::map<std::string,RetrySchedule *>::iterator it;
	for(it=schedules.begin();it!=schedules.end();++it)
		delete it->second;
	
	schedules.clear();
	
	// Update
	DB db;
	DB db2(&db);
	db.Query("SELECT schedule_name FROM t_schedule");
	
	while(db.FetchRow())
	{
		std::string schedule_name(db.GetField(0));
		schedules[schedule_name] = new RetrySchedule(&db2,db.GetField(0));
	}
	
	pthread_mutex_unlock(&lock);
}

RetrySchedule RetrySchedules::GetRetrySchedule(const char *name)
{
	pthread_mutex_lock(&lock);
	
	std::map<std::string,RetrySchedule *>::iterator it;
	it = schedules.find(name);
	if(it==schedules.end())
	{
		pthread_mutex_unlock(&lock);
		
		throw Exception("RetrySchedules","Unable to find schedule");
	}
	
	RetrySchedule schedule = *it->second;
	
	pthread_mutex_unlock(&lock);
	
	return schedule;
}
