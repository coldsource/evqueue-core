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
#include <Exception.h>
#include <DB.h>
#include <Configuration.h>
#include <Logger.h>
#include <WorkflowInstance.h>
#include <Sockets.h>
#include <FileManager.h>
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

using namespace std;

Notification::Notification(DB *db,unsigned int notification_id)
{
	db->QueryPrintf("SELECT nt.notification_type_binary, nt.notification_type_name, n.notification_parameters FROM t_notification n, t_notification_type nt WHERE nt.notification_type_id=n.notification_type_id AND n.notification_id=%i",&notification_id);
	
	if(!db->FetchRow())
		throw Exception("Notification","Unknown notification");
	
	notification_monitor_path = Configuration::GetInstance()->Get("notifications.monitor.path");
	
	const char *ptr = db->GetField(0);
	if(ptr[0]=='/')
		notification_binary = ptr;
	else
		notification_binary = Configuration::GetInstance()->Get("notifications.tasks.directory")+"/"+ptr;
	
	notification_name = db->GetField(1);
	notification_configuration = db->GetField(2);
}

void Notification::Call(WorkflowInstance *workflow_instance)
{
	int pipe_fd[2];
	
	pipe(pipe_fd);
	
	pid_t pid = fork();
	if(pid==0)
	{
		setsid();
		
		dup2(pipe_fd[0],STDIN_FILENO);
		close(pipe_fd[1]);
		
		setenv("EVQUEUE_WORKING_DIRECTORY",Configuration::GetInstance()->Get("notifications.monitor.path").c_str(),true);
		
		char str_timeout[16],str_instance_id[16],str_errors[16];
		sprintf(str_timeout,"%d",Configuration::GetInstance()->GetInt("notifications.tasks.timeout"));
		sprintf(str_instance_id,"%d",workflow_instance->GetInstanceID());
		sprintf(str_errors,"%d",workflow_instance->GetErrors());
		
		execl(notification_monitor_path.c_str(),notification_monitor_path.c_str(),notification_binary.c_str(),str_timeout,str_instance_id,str_errors,(char *)0);
		
		ipc_send_exit_msg(2,0,-1);
		exit(-1);
	}
	
	if(pid<0)
	{
		Logger::Log(LOG_WARNING,"[ WID %d ] Unable to execute notification task '%s' : could not fork monitor",workflow_instance->GetInstanceID(),notification_name.c_str());
		
		close(pipe_fd[0]);
		close(pipe_fd[1]);
		
		return;
	}
	
	// Pipe configuration data
	write(pipe_fd[1],notification_configuration.c_str(),notification_configuration.length());
	close(pipe_fd[1]);
	close(pipe_fd[0]);
}

void Notification::PutFile(const std::string &filename,const std::string &data)
{
	FileManager::PutFile(Configuration::GetInstance()->Get("notifications.tasks.directory"),filename,data,FileManager::FILETYPE_BINARY);
}

void Notification::PutFileConf(const std::string &filename,const std::string &data)
{
	FileManager::PutFile(Configuration::GetInstance()->Get("notifications.tasks.directory")+"/conf",filename,data,FileManager::FILETYPE_CONF);
}

void Notification::RemoveFile(const std::string &filename)
{
	FileManager::RemoveFile(Configuration::GetInstance()->Get("notifications.tasks.directory"),filename);
}