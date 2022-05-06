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

#include <Notification/Notifications.h>
#include <Notification/Notification.h>
#include <Notification/NotificationType.h>
#include <DB/DB.h>
#include <Exception/Exception.h>
#include <Logger/Logger.h>
#include <Configuration/ConfigurationEvQueue.h>
#include <WorkflowInstance/WorkflowInstance.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <Cluster/Cluster.h>
#include <User/User.h>
#include <Logger/LoggerNotifications.h>
#include <API/QueryHandlers.h>

#include <string.h>

Notifications *Notifications::instance = 0;

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	qh->RegisterHandler("notifications", Notifications::HandleQuery);
	return (APIAutoInit *)new Notifications();
});

using namespace std;
using nlohmann::json;

Notifications::Notifications():APIObjectList()
{
	instance = this;
	
	max_concurrency = ConfigurationEvQueue::GetInstance()->GetInt("notifications.tasks.concurrency");
	
	logs_directory = ConfigurationEvQueue::GetInstance()->Get("notifications.logs.directory");
	logs_maxsize = ConfigurationEvQueue::GetInstance()->GetSize("notifications.logs.maxsize");
	logs_delete = ConfigurationEvQueue::GetInstance()->GetBool("notifications.logs.delete");
	
	uid = 1;
	
	Reload(false);
}

Notifications::~Notifications()
{
}

void Notifications::Reload(bool notify)
{
	Logger::Log(LOG_NOTICE,"Reloading notifications definitions");
	
	unique_lock<mutex> llock(lock);
	
	clear();
	
	// Update
	DB db;
	DB db2(&db);
	db.Query("SELECT notification_id FROM t_notification");
	
	while(db.FetchRow())
		add(db.GetFieldInt(0),"",new Notification(&db2,db.GetFieldInt(0)));
	
	llock.unlock();
	
	if(notify)
	{
		// Notify cluster
		Cluster::GetInstance()->Notify("<control action='reload' module='notifications' notify='no' />\n");
	}
}

void Notifications::Call(unsigned int notification_id, const std::string &notif_instance_name, const vector<string> &params, const json &j_data)
{	
	try
	{
		Notification notification = Get(notification_id);
		
		unique_lock<mutex> llock(lock);
		
		if(notification_instances.size()<max_concurrency)
		{
			pid_t pid = notification.Call(notif_instance_name, uid, params, j_data);
			if(pid>0)
				notification_instances.insert(pair<pid_t,st_notification_instance>(pid,{uid++, notif_instance_name, notification.GetID(),notification.GetName()}));
		}
		else
		{
			Logger::Log(
				LOG_WARNING,
				"Maximum concurrency reached for notifications calls, dropping call for notification '%s' of %s",
				notification.GetName().c_str(),
				notif_instance_name.c_str()
			);
		}
	}
	catch(Exception &e)
	{
		Logger::Log(LOG_ERR,"Exception during notifications processing : %s",e.error.c_str());
	}
}

void Notifications::Exit(pid_t pid, int status, char retcode)
{
	try
	{
		unique_lock<mutex> llock(lock);
		
		map<pid_t,st_notification_instance>::iterator it;
		it = notification_instances.find(pid);
		if(it==notification_instances.end())
		{
			Logger::Log(LOG_WARNING,"[ Notifications ] Got exit from pid %d but could not find corresponding notification",pid);
			return;
		}
		
		st_notification_instance ni = it->second;
		
		if(status==0)
		{
			if(retcode!=0)
			{
				Logger::Log(LOG_WARNING,"Notification task '%s' (pid %d) for %s returned code %d",ni.notification_type.c_str(),pid,ni.notif_instance_name.c_str(),retcode);
				
				// Read and store log file
				store_log(pid,ni.uid,ni.notification_id);
			}
			else
				Logger::Log(LOG_NOTICE,"Notification task '%s' (pid %d) for %s executed successuflly",ni.notification_type.c_str(),pid,ni.notif_instance_name.c_str());
			
			if(logs_delete)
			{
				string filename = logs_directory+"/notif_"+to_string(ni.uid)+"_"+to_string(ni.notification_id)+".stderr";
				if(unlink(filename.c_str())!=0)
					Logger::Log(LOG_WARNING,"Error removing log file "+filename);
			}
		}
		else if(status==1)
			Logger::Log(LOG_WARNING,"Notification task '%s' (pid %d) for %s was killed",ni.notification_type.c_str(),pid,ni.notif_instance_name.c_str());
		else if(status==2)
			Logger::Log(LOG_WARNING,"Notification task '%s' (pid %d) for %s timed out",ni.notification_type.c_str(),pid,ni.notif_instance_name.c_str());
		else if(status==3)
			Logger::Log(LOG_ALERT,"Notification task '%s' (pid %d) for %s could not be forked",ni.notification_type.c_str(),pid,ni.notif_instance_name.c_str());
		
		notification_instances.erase(pid);
	}
	catch(Exception &e)
	{
		Logger::Log(LOG_ERR,"Exception on notification script exit : "+e.error+"("+e.context+")");
	}
}

bool Notifications::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	Notifications *notifications = Notifications::GetInstance();
	
	const string action = query->GetRootAttribute("action");
	
	if(action=="list")
	{
		unique_lock<mutex> llock(notifications->lock);
		
		const string scope = query->GetRootAttribute("scope", "*");
		
		for(auto it = notifications->objects_id.begin(); it!=notifications->objects_id.end(); it++)
		{
			if(scope!="*" && it->second->GetType().GetScope()!=scope)
				continue;
			
			DOMElement node = (DOMElement)response->AppendXML("<notification />");
			node.setAttribute("id",to_string(it->second->GetID()));
			node.setAttribute("type_id",std::to_string(it->second->GetTypeID()));
			node.setAttribute("name",it->second->GetName());
			node.setAttribute("scope",it->second->GetType().GetScope());
		}
		
		return true;
	}
	
	return false;
}

void Notifications::store_log(pid_t pid, unsigned int uid, unsigned int notification_id)
{
	string filename = logs_directory+"/notif_"+to_string(uid)+"_"+to_string(notification_id)+".stderr";
	FILE *f = fopen(filename.c_str(),"r");
	
	if(!f)
	{
		Logger::Log(LOG_WARNING,"Unable to read notification log file "+filename);
		return;
	}
	
	fseek(f,0,SEEK_END);
	size_t log_size = ftell(f);
	if(log_size>logs_maxsize)
	{
		log_size = logs_maxsize;
		Logger::Log(LOG_NOTICE,"Truncated log file "+filename);
	}
	
	char *buf = new char[log_size+1];
	fseek(f,0,SEEK_SET);
	size_t read_size = fread(buf,1,log_size,f);
	fclose(f);
	
	if(read_size!=log_size)
		Logger::Log(LOG_WARNING,"Error reading log file "+filename);
	
	buf[read_size] = '\0';
	
	LoggerNotifications::GetInstance()->Log(pid,buf);
	
	delete[] buf;
}
