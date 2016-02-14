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
#include <sha1.h>

#include <string.h>

Tasks *Tasks::instance = 0;

using namespace std;

Tasks::Tasks()
{
	instance = this;
	
	pthread_mutex_init(&lock, NULL);
	
	Reload();
}

Tasks::~Tasks()
{
	// Clean current tasks
	std::map<std::string,Task *>::iterator it;
	for(it=tasks.begin();it!=tasks.end();++it)
		delete it->second;
	
	tasks.clear();
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

void Tasks::SyncBinaries(void)
{
	Logger::Log(LOG_INFO,"[ Tasks ] Syncing binaries");
	
	pthread_mutex_lock(&lock);
	
	DB db;
	
	// Load tasks from database
	db.Query("SELECT task_binary, task_binary_content FROM t_task WHERE task_binary_content IS NOT NULL");
	
	struct sha1_ctx ctx;
	char db_hash[20];
	string file_hash;
	
	while(db.FetchRow())
	{
		// Compute database SHA1 hash
		sha1_init_ctx(&ctx);
		sha1_process_bytes(db.GetField(1),db.GetFieldLength(1),&ctx);
		sha1_finish_ctx(&ctx,db_hash);
		
		// Compute file SHA1 hash
		try
		{
			Task::GetFileHash(db.GetField(0),file_hash);
		}
		catch(Exception &e)
		{
			Logger::Log(LOG_NOTICE,"[ Tasks ] Task %s was not found creating it",db.GetField(0));
			
			Task::PutFile(db.GetField(0),string(db.GetField(1),db.GetFieldLength(1)),false);
			continue;
		}
		
		if(memcmp(file_hash.c_str(),db_hash,20)==0)
		{
			Logger::Log(LOG_NOTICE,"[ Tasks ] Task %s hash matches DB, skipping",db.GetField(0));
			continue;
		}
		
		Logger::Log(LOG_NOTICE,"[ Tasks ] Task %s hash does not match DB, replacing",db.GetField(0));
		
		Task::RemoveFile(db.GetField(0));
		Task::PutFile(db.GetField(0),string(db.GetField(1),db.GetFieldLength(1)),false);
	}
	
	pthread_mutex_unlock(&lock);
}

Task Tasks::GetTask(const string &name)
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
