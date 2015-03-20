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

#include <Tasks.h>
#include <Task.h>
#include <DB.h>
#include <Exception.h>
#include <Logger.h>

#include <string.h>

Tasks *Tasks::instance = 0;

Tasks::Tasks()
{
	instance = this;
	
	pthread_mutex_init(&lock, NULL);
	
	Reload();
}

void Tasks::Reload(void)
{
	Logger::Log(LOG_NOTICE,"[ Tasks ] Reloading tasks definitions");
	
	pthread_mutex_lock(&lock);
	
	// Clean current tasks
	std::map<std::string,Task *>::iterator it;
	for(it=tasks.begin();it!=tasks.end();++it)
		delete it->second;
	
	tasks.clear();
	
	// Update
	DB db;
	DB db2(&db);
	db.Query("SELECT task_name FROM t_task");
	
	while(db.FetchRow())
	{
		std::string task_name(db.GetField(0));
		tasks[task_name] = new Task(&db2,db.GetField(0));
	}
	
	pthread_mutex_unlock(&lock);
}

Task Tasks::GetTask(const char *name)
{
	pthread_mutex_lock(&lock);
	
	std::map<std::string,Task *>::iterator it;
	it = tasks.find(name);
	if(it==tasks.end())
	{
		pthread_mutex_unlock(&lock);
		
		throw Exception("Tasks","Unable to find task");
	}
	
	Task task = *it->second;
	
	pthread_mutex_unlock(&lock);
	
	return task;
}
