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

Tasks *Tasks::instance = 0;

using namespace std;

Tasks::Tasks():APIObjectList()
{
	instance = this;
	
	Reload(false);
}

Tasks::~Tasks()
{
}

void Tasks::Reload(bool notify)
{
	Logger::Log(LOG_NOTICE,"Reloading tasks definitions");
	
	unique_lock<mutex> llock(lock);
	
	// Clean current tasks
	clear();
	
	// Update
	DB db;
	DB db2(&db);
	db.Query("SELECT task_id,task_name FROM t_task");
	
	while(db.FetchRow())
		add(db.GetFieldInt(0),db.GetField(1),new Task(&db2,db.GetField(1)));
	
	if(notify)
	{
		// Notify cluster
		Cluster::GetInstance()->Notify("<control action='reload' module='tasks' notify='no' />\n");
	}
}

void Tasks::SyncBinaries(bool notify)
{
	Logger::Log(LOG_NOTICE,"[ Tasks ] Syncing binaries");
	
	unique_lock<mutex> llock(lock);
	
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
		sha1_process_bytes(db.GetField(1).c_str(),db.GetFieldLength(1),&ctx);
		sha1_finish_ctx(&ctx,db_hash);
		
		// Compute file SHA1 hash
		try
		{
			Task::GetFileHash(db.GetField(0),file_hash);
		}
		catch(Exception &e)
		{
			Logger::Log(LOG_NOTICE,"[ Tasks ] Task "+db.GetField(0)+" was not found creating it");
			
			Task::PutFile(db.GetField(0),string(db.GetField(1),db.GetFieldLength(1)),false);
			continue;
		}
		
		if(memcmp(file_hash.c_str(),db_hash,20)==0)
		{
			Logger::Log(LOG_NOTICE,"[ Tasks ] Task "+db.GetField(0)+" hash matches DB, skipping");
			continue;
		}
		
		Logger::Log(LOG_NOTICE,"[ Tasks ] Task "+db.GetField(0)+" hash does not match DB, replacing");
		
		Task::RemoveFile(db.GetField(0));
		Task::PutFile(db.GetField(0),string(db.GetField(1),db.GetFieldLength(1)),false);
	}
	
	llock.unlock();
	
	if(notify)
	{
		// Notify cluster
		Cluster::GetInstance()->Notify("<control action='synctasks' notify='no' />\n");
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
		unique_lock<mutex> llock(tasks->lock);
		
		for(auto it = tasks->objects_name.begin(); it!=tasks->objects_name.end(); it++)
		{
			Task task = *it->second;
			DOMElement node = (DOMElement)response->AppendXML("<task />");
			node.setAttribute("id",to_string(task.GetID()));
			node.setAttribute("name",task.GetName());
			node.setAttribute("user",task.GetUser());
			node.setAttribute("host",task.GetHost());
			node.setAttribute("binary",task.GetBinary());
			node.setAttribute("parameters_mode",task.GetParametersMode()==task_parameters_mode::ENV?"ENV":"CMDLINE");
			node.setAttribute("output_method",task.GetOutputMethod()==task_output_method::TEXT?"TEXT":"XML");
			node.setAttribute("group",task.GetGroup());
			node.setAttribute("comment",task.GetComment());
			node.setAttribute("modified",task.GetIsModified()?"1":"0");
			node.setAttribute("lastcommit",task.GetLastCommit());
		}
		
		return true;
	}
	
	return false;
}
