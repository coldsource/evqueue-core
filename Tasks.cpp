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
#include <SocketQuerySAX2Handler.h>
#include <QueryResponse.h>
#include <sha1.h>

#include <string.h>

#include <xqilla/xqilla-dom3.hpp>

Tasks *Tasks::instance = 0;

using namespace std;
using namespace xercesc;

Tasks::Tasks()
{
	instance = this;
	
	pthread_mutex_init(&lock, NULL);
	
	Reload();
}

Tasks::~Tasks()
{
	// Clean current tasks
	for(auto it=tasks_name.begin();it!=tasks_name.end();++it)
		delete it->second;
	
	tasks_id.clear();
	tasks_name.clear();
}

void Tasks::Reload(void)
{
	Logger::Log(LOG_NOTICE,"[ Tasks ] Reloading tasks definitions");
	
	pthread_mutex_lock(&lock);
	
	// Clean current tasks
	for(auto it=tasks_name.begin();it!=tasks_name.end();++it)
		delete it->second;
	
	tasks_id.clear();
	tasks_name.clear();
	
	// Update
	DB db;
	DB db2(&db);
	db.Query("SELECT task_id,task_name FROM t_task");
	
	while(db.FetchRow())
	{
		std::string task_name(db.GetField(1));
		Task * task = new Task(&db2,db.GetField(1));
		
		tasks_id[db.GetFieldInt(0)] = task;
		tasks_name[task_name] = task;
	}
	
	pthread_mutex_unlock(&lock);
}

void Tasks::SyncBinaries(void)
{
	Logger::Log(LOG_NOTICE,"[ Tasks ] Syncing binaries");
	
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

Task Tasks::GetTask(unsigned int id)
{
	pthread_mutex_lock(&lock);
	
	auto it = tasks_id.find(id);
	if(it==tasks_id.end())
	{
		pthread_mutex_unlock(&lock);
		
		throw Exception("Tasks","Unable to find task");
	}
	
	Task task = *it->second;
	
	pthread_mutex_unlock(&lock);
	
	return task;
}

Task Tasks::GetTask(const string &name)
{
	pthread_mutex_lock(&lock);
	
	auto it = tasks_name.find(name);
	if(it==tasks_name.end())
	{
		pthread_mutex_unlock(&lock);
		
		throw Exception("Tasks","Unable to find task");
	}
	
	Task task = *it->second;
	
	pthread_mutex_unlock(&lock);
	
	return task;
}

bool Tasks::Exists(unsigned int id)
{
	pthread_mutex_lock(&lock);
	
	auto it = tasks_id.find(id);
	if(it==tasks_id.end())
	{
		pthread_mutex_unlock(&lock);
		
		return false;
	}
	
	pthread_mutex_unlock(&lock);
	
	return true;
}

bool Tasks::HandleQuery(SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	Tasks *tasks = Tasks::GetInstance();
	
	const std::map<std::string,std::string> attrs = saxh->GetRootAttributes();
	
	auto it_action = attrs.find("action");
	if(it_action==attrs.end())
		return false;
	
	if(it_action->second=="list")
	{
		pthread_mutex_lock(&tasks->lock);
		
		for(auto it = tasks->tasks_name.begin(); it!=tasks->tasks_name.end(); it++)
		{
			Task task = *it->second;
			DOMElement *node = (DOMElement *)response->AppendXML("<task />");
			node->setAttribute(X("id"),X(std::to_string(task.GetID()).c_str()));
			node->setAttribute(X("name"),X(task.GetName().c_str()));
			node->setAttribute(X("user"),X(task.GetUser().c_str()));
			node->setAttribute(X("host"),X(task.GetHost().c_str()));
			node->setAttribute(X("binary"),X(task.GetBinary().c_str()));
			node->setAttribute(X("parameters_mode"),task.GetParametersMode()==task_parameters_mode::ENV?X("ENV"):X("CMDLINE"));
			node->setAttribute(X("group"),X(task.GetGroup().c_str()));
			node->setAttribute(X("comment"),X(task.GetComment().c_str()));
		}
		
		pthread_mutex_unlock(&tasks->lock);
		
		return true;
	}
	
	return false;
}
