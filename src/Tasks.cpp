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
#include <Cluster.h>
#include <User.h>
#include <sha1.h>

#include <string.h>

#include <xqilla/xqilla-dom3.hpp>

Tasks *Tasks::instance = 0;

using namespace std;
using namespace xercesc;

Tasks::Tasks():APIObjectList()
{
	instance = this;
	
	pthread_mutex_init(&lock, NULL);
	
	Reload(false);
}

Tasks::~Tasks()
{
}

void Tasks::Reload(bool notify)
{
	Logger::Log(LOG_NOTICE,"[ Tasks ] Reloading tasks definitions");
	
	pthread_mutex_lock(&lock);
	
	// Clean current tasks
	clear();
	
	// Update
	DB db;
	DB db2(&db);
	db.Query("SELECT task_id,task_name FROM t_task");
	
	while(db.FetchRow())
		add(db.GetFieldInt(0),db.GetField(1),new Task(&db2,db.GetField(1)));
	
	pthread_mutex_unlock(&lock);
	
	if(notify)
	{
		// Notify cluster
		Cluster::GetInstance()->ExecuteCommand("<control action='reload' module='tasks' notify='no' />\n");
	}
}

void Tasks::SyncBinaries(bool notify)
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
	
	if(notify)
	{
		// Notify cluster
		Cluster::GetInstance()->ExecuteCommand("<control action='synctasks' notify='no' />\n");
	}
}

bool Tasks::HandleQuery(const User &user, SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	Tasks *tasks = Tasks::GetInstance();
	
	const string action = saxh->GetRootAttribute("action");
	
	if(action=="list")
	{
		pthread_mutex_lock(&tasks->lock);
		
		for(auto it = tasks->objects_name.begin(); it!=tasks->objects_name.end(); it++)
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
