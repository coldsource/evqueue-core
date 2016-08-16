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

#include <Notification.h>
#include <Notifications.h>
#include <NotificationType.h>
#include <NotificationTypes.h>
#include <Workflows.h>
#include <Exception.h>
#include <DB.h>
#include <Configuration.h>
#include <Logger.h>
#include <WorkflowInstance.h>
#include <Sockets.h>
#include <SocketQuerySAX2Handler.h>
#include <QueryResponse.h>
#include <tools.h>
#include <global.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include <xqilla/xqilla-dom3.hpp>

using namespace std;
using namespace xercesc;

Notification::Notification(DB *db,unsigned int notification_id)
{
	id = notification_id;
	
	db->QueryPrintf("SELECT notification_type_id,notification_name,notification_parameters FROM t_notification WHERE notification_id=%i",&notification_id);
	
	if(!db->FetchRow())
		throw Exception("Notification","Unknown notification");
	
	unix_socket_path = Configuration::GetInstance()->Get("network.bind.path");
	
	notification_monitor_path = Configuration::GetInstance()->Get("notifications.monitor.path");
	
	type_id = db->GetFieldInt(0);
	
	NotificationType notification_type = NotificationTypes::GetInstance()->Get(type_id);
	
	if(notification_type.GetName().at(0)=='/')
		notification_binary = notification_type.GetName();
	else
		notification_binary = Configuration::GetInstance()->Get("notifications.tasks.directory")+"/"+notification_type.GetName();
	
	notification_name = db->GetField(1);
	notification_configuration = db->GetField(2);
}

pid_t Notification::Call(WorkflowInstance *workflow_instance)
{
	int pipe_fd[2];
	
	pipe(pipe_fd);
	
	pid_t pid = fork();
	if(pid==0)
	{
		setsid();
		
		dup2(pipe_fd[0],STDIN_FILENO);
		close(pipe_fd[1]);
		
		setenv("EVQUEUE_IPC_QID",Configuration::GetInstance()->Get("core.ipc.qid").c_str(),true);
		
		setenv("EVQUEUE_WORKING_DIRECTORY",Configuration::GetInstance()->Get("notifications.monitor.path").c_str(),true);
		
		char str_timeout[16],str_instance_id[16],str_errors[16];
		sprintf(str_timeout,"%d",Configuration::GetInstance()->GetInt("notifications.tasks.timeout"));
		sprintf(str_instance_id,"%d",workflow_instance->GetInstanceID());
		sprintf(str_errors,"%d",workflow_instance->GetErrors());
		
		execl(notification_monitor_path.c_str(),notification_monitor_path.c_str(),notification_binary.c_str(),str_timeout,str_instance_id,str_errors,unix_socket_path.c_str(),(char *)0);
		
		tools_send_exit_msg(2,0,-1);
		exit(-1);
	}
	
	if(pid<0)
	{
		Logger::Log(LOG_WARNING,"[ WID %d ] Unable to execute notification task '%s' : could not fork monitor",workflow_instance->GetInstanceID(),notification_name.c_str());
		
		close(pipe_fd[0]);
		close(pipe_fd[1]);
		
		return pid;
	}
	
	// Pipe configuration data
	write(pipe_fd[1],notification_configuration.c_str(),notification_configuration.length());
	close(pipe_fd[1]);
	close(pipe_fd[0]);
	
	return pid;
}

void Notification::Get(unsigned int id,QueryResponse *response)
{
	Notification notification = Notifications::GetInstance()->Get(id);
	
	DOMElement *node = (DOMElement *)response->AppendXML("<notification />");
	node->setAttribute(X("type_id"),X(to_string(notification.GetTypeID()).c_str()));
	node->setAttribute(X("name"),X(notification.GetName().c_str()));
	node->setAttribute(X("parameters"),X(notification.GetConfiguration().c_str()));
}

void Notification::Create(unsigned int type_id,const std::string &name, const std::string parameters)
{
	create_edit_check(type_id,name,parameters);
	
	DB db;
	db.QueryPrintf("INSERT INTO t_notification(notification_type_id,notification_name,notification_parameters) VALUES(%i,%s,%s)",&type_id,name.c_str(),parameters.c_str());
}

void Notification::Edit(unsigned int id,unsigned int type_id,const std::string &name, const std::string parameters)
{
	create_edit_check(type_id,name,parameters);
	
	if(!Notifications::GetInstance()->Exists(id))
		throw Exception("Notification","Unable to find notification");
	
	DB db;
	db.QueryPrintf("UPDATE t_notification SET notification_type_id=%i,notification_name=%s,notification_parameters=%s WHERE notification_id=%i",type_id,name.c_str(),parameters.c_str(),&id);
}

void Notification::Delete(unsigned int id)
{
	DB db;
	db.QueryPrintf("DELETE FROM t_notification WHERE notification_id=%i",&id);
	
	if(db.AffectedRows()==0)
		throw Exception("Notification","Unable to find notification");
}

void Notification::create_edit_check(unsigned int type_id,const std::string &name, const std::string parameters)
{
	if(name.length()==0)
		throw Exception("Notification","Name cannot be empty");
	
	if(!NotificationTypes::GetInstance()->Exists(type_id))
		throw Exception("Notification","Unknown notification type ID");
}

bool Notification::HandleQuery(SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	const string action = saxh->GetRootAttribute("action");
	
	if(action=="get")
	{
		unsigned int id = saxh->GetRootAttributeInt("id");
		
		Get(id,response);
		
		return true;
	}
	else if(action=="create" || action=="edit")
	{
		unsigned int type_id = saxh->GetRootAttributeInt("type_id");
		string name = saxh->GetRootAttribute("name");
		string parameters = saxh->GetRootAttribute("parameters");
		
		if(action=="create")
			Create(type_id, name, parameters);
		else
		{
			unsigned int id = saxh->GetRootAttributeInt("id");
			
			Edit(id, type_id, name, parameters);
		}
		
		Notifications::GetInstance()->Reload();
		Workflows::GetInstance()->Reload();
		
		return true;
	}
	else if(action=="delete")
	{
		unsigned int id = saxh->GetRootAttributeInt("id");
		
		Delete(id);
		
		Notifications::GetInstance()->Reload();
		Workflows::GetInstance()->Reload();
		
		return true;
	}
	
	return false;
}