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
#include <ConfigurationEvQueue.h>
#include <Logger.h>
#include <WorkflowInstance.h>
#include <Sockets.h>
#include <SocketQuerySAX2Handler.h>
#include <QueryResponse.h>
#include <base64.h>
#include <User.h>
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
#include <fcntl.h>

using namespace std;

Notification::Notification(DB *db,unsigned int notification_id)
{
	id = notification_id;
	
	db->QueryPrintf("SELECT notification_type_id,notification_name,notification_parameters FROM t_notification WHERE notification_id=%i",&notification_id);
	
	if(!db->FetchRow())
		throw Exception("Notification","Unknown notification");
	
	unix_socket_path = ConfigurationEvQueue::GetInstance()->Get("network.bind.path");
	
	notification_monitor_path = ConfigurationEvQueue::GetInstance()->Get("notifications.monitor.path");
	
	logs_directory = ConfigurationEvQueue::GetInstance()->Get("notifications.logs.directory");
	
	type_id = db->GetFieldInt(0);
	
	NotificationType notification_type = NotificationTypes::GetInstance()->Get(type_id);
	
	if(notification_type.GetName().at(0)=='/')
		notification_binary = notification_type.GetName();
	else
		notification_binary = ConfigurationEvQueue::GetInstance()->Get("notifications.tasks.directory")+"/"+notification_type.GetName();
	
	notification_name = db->GetField(1);
	
	// Build confiuration JSON
	notification_configuration = db->GetField(2);
	plugin_configuration = notification_type.GetConfiguration();
}

pid_t Notification::Call(WorkflowInstance *workflow_instance)
{
	int pipe_fd[2];
	
	if(pipe(pipe_fd)!=0)
	{
		Logger::Log(LOG_WARNING,"[ WID %d ] Unable to execute notification task '%s' : could not create pipe",workflow_instance->GetInstanceID(),notification_name.c_str());
		return -1;
	}
	
	pid_t pid = fork();
	if(pid==0)
	{
		setsid();
		
		dup2(pipe_fd[0],STDIN_FILENO);
		close(pipe_fd[1]);
		
		// Redirect stderr to file
		string log_filename_stderr = logs_directory+"/notif_stderr_"+to_string(getpid());
		int fno_stderr = open(log_filename_stderr.c_str(),O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR);
		dup2(fno_stderr,STDERR_FILENO);
		
		setenv("EVQUEUE_IPC_QID",ConfigurationEvQueue::GetInstance()->Get("core.ipc.qid").c_str(),true);
		
		setenv("EVQUEUE_WORKING_DIRECTORY",ConfigurationEvQueue::GetInstance()->Get("notifications.tasks.directory").c_str(),true);
		
		char str_timeout[16],str_instance_id[16],str_errors[16];
		sprintf(str_timeout,"%d",ConfigurationEvQueue::GetInstance()->GetInt("notifications.tasks.timeout"));
		sprintf(str_instance_id,"%d",workflow_instance->GetInstanceID());
		sprintf(str_errors,"%d",workflow_instance->GetErrors());
		
		execl(notification_monitor_path.c_str(),notification_monitor_path.c_str(),notification_binary.c_str(),str_timeout,str_instance_id,str_errors,unix_socket_path.c_str(),(char *)0);
		Logger::Log(LOG_ERR,"Unable to execute notification monitor");
		
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
	
	string configuration;
	configuration += "{\"pluginconf\":";
	configuration += plugin_configuration;
	configuration += ",\"notificationconf\":";
	configuration += notification_configuration;
	configuration += ",\"instance\":\"";
	configuration += json_escape(workflow_instance->GetDOM()->Serialize(workflow_instance->GetDOM()->getDocumentElement()));
	configuration += "\"}";
	
	// Pipe configuration data
	if(write(pipe_fd[1],configuration.c_str(),configuration.length())!=configuration.length())
		Logger::Log(LOG_WARNING,"[ WID %d ] Unable to send configuration to notification task '%s' : error writing to pipe",workflow_instance->GetInstanceID(),notification_name.c_str());
	
	close(pipe_fd[1]);
	close(pipe_fd[0]);
	
	return pid;
}

void Notification::Get(unsigned int id,QueryResponse *response)
{
	Notification notification = Notifications::GetInstance()->Get(id);
	
	DOMElement node = (DOMElement)response->AppendXML("<notification />");
	node.setAttribute("type_id",to_string(notification.GetTypeID()));
	node.setAttribute("name",notification.GetName());
	
	string  parameters_base64;
	base64_encode_string(notification.GetConfiguration(),parameters_base64);
	node.setAttribute("parameters",parameters_base64);
}

void Notification::Create(unsigned int type_id,const std::string &name, const std::string parameters)
{
	create_edit_check(type_id,name,parameters);
	
	if(!NotificationTypes::GetInstance()->Exists(type_id))
		throw Exception("Notification","Unknown notification type ID","UNKNOWN_NOTIFICATION_TYPE");
	
	DB db;
	db.QueryPrintf("INSERT INTO t_notification(notification_type_id,notification_name,notification_parameters) VALUES(%i,%s,%s)",&type_id,&name,&parameters);
}

void Notification::Edit(unsigned int id,const std::string &name, const std::string parameters)
{
	if(!Notifications::GetInstance()->Exists(id))
		throw Exception("Notification","Unable to find notification","UNKNOWN_NOTIFICATION");
	
	create_edit_check(0,name,parameters);
	
	DB db;
	db.QueryPrintf("UPDATE t_notification SET notification_name=%s,notification_parameters=%s WHERE notification_id=%i",&name,&parameters,&id);
}

void Notification::Delete(unsigned int id)
{
	if(!Notifications::GetInstance()->Exists(id))
		throw Exception("Notification","Unable to find notification","UNKNOWN_NOTIFICATION");
	
	DB db;
	
	db.StartTransaction();
	
	db.QueryPrintf("DELETE FROM t_notification WHERE notification_id=%i",&id);
	
	db.QueryPrintf("DELETE FROM t_workflow_notification WHERE notification_id=%i",&id);
	
	db.CommitTransaction();
}

string Notification::json_escape(const string &str)
{
	string escaped_str;
	for(int i=0;i<str.length();i++)
	{
		if(str[i]=='\b')
			escaped_str+="\\b";
		else if(str[i]=='\f')
			escaped_str+="\\f";
		else if(str[i]=='\r')
			escaped_str+="\\r";
		else if(str[i]=='\n')
			escaped_str+="\\n";
		else if(str[i]=='\t')
			escaped_str+="\\t";
		else if(str[i]=='\"')
			escaped_str+="\\\"";
		else if(str[i]=='\\')
			escaped_str+="\\\\";
		else
			escaped_str+=str[i];
	}
	
	return escaped_str;
}

void Notification::create_edit_check(unsigned int type_id,const std::string &name, const std::string parameters)
{
	if(name.length()==0)
		throw Exception("Notification","Name cannot be empty","INVALID_PARAMETER");
}

bool Notification::HandleQuery(const User &user, SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	const string action = saxh->GetRootAttribute("action");
	
	if(action=="get")
	{
		unsigned int id = saxh->GetRootAttributeInt("id");
		
		Get(id,response);
		
		return true;
	}
	else if(action=="create" || action=="edit")
	{
		string name = saxh->GetRootAttribute("name");
		string parameters_base64 = saxh->GetRootAttribute("parameters");
		string parameters;
		if(parameters_base64.length())
			base64_decode_string(parameters,parameters_base64);
		
		if(action=="create")
		{
			unsigned int type_id = saxh->GetRootAttributeInt("type_id");
			
			Create(type_id, name, parameters);
		}
		else
		{
			unsigned int id = saxh->GetRootAttributeInt("id");
			
			Edit(id, name, parameters);
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
