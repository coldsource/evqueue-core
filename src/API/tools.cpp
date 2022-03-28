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

#include <API/tools.h>
#include <Process/tools_ipc.h>
#include <global.h>
#include <Logger/Logger.h>
#include <Schedule/WorkflowScheduler.h>
#include <Schedule/RetrySchedules.h>
#include <Workflow/Workflows.h>
#include <Notification/Notifications.h>
#include <Notification/NotificationTypes.h>
#include <Schedule/Retrier.h>
#include <Queue/QueuePool.h>
#include <Configuration/ConfigurationEvQueue.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <API/Statistics.h>
#include <WorkflowInstance/WorkflowInstances.h>
#include <User/Users.h>
#include <User/User.h>
#include <Tag/Tags.h>
#include <Logs/Channels.h>
#include <Logs/ChannelGroups.h>
#include <Exception/Exception.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

using namespace std;

void tools_config_reload(const std::string &module,bool notify)
{
	bool module_is_valid = false;
	
	if(module=="all" || module=="scheduler")
	{
		WorkflowScheduler *scheduler = WorkflowScheduler::GetInstance();
		scheduler->Reload(notify);
		module_is_valid = true;
	}
	
	if(module=="all" || module=="retry_schedules")
	{
		RetrySchedules *retry_schedules = RetrySchedules::GetInstance();
		retry_schedules->Reload(notify);
		module_is_valid = true;
	}
	
	if(module=="all" || module=="workflows")
	{
		Workflows *workflows = Workflows::GetInstance();
		workflows->Reload(notify);
		module_is_valid = true;
	}
	
	if(module=="all" || module=="notifications")
	{
		NotificationTypes::GetInstance()->Reload(notify);
		Notifications::GetInstance()->Reload(notify);
		module_is_valid = true;
	}
	
	if(module=="all" || module=="queuepool")
	{
		QueuePool *qp = QueuePool::GetInstance();
		qp->Reload(notify);
		module_is_valid = true;
	}
	
	if(module=="all" || module=="users")
	{
		Users *users = Users::GetInstance();
		users->Reload(notify);
		module_is_valid = true;
	}
	
	if(module=="all" || module=="tags")
	{
		Tags *tags = Tags::GetInstance();
		tags->Reload(notify);
		module_is_valid = true;
	}
	
	if(module=="all" || module=="channels")
	{
		Channels *channels = Channels::GetInstance();
		channels->Reload(notify);
		module_is_valid = true;
	}
	
	if(module=="all" || module=="channelgroups")
	{
		ChannelGroups *channelgroups = ChannelGroups::GetInstance();
		channelgroups->Reload(notify);
		module_is_valid = true;
	}
	
	if(!module_is_valid)
		throw Exception("Control","Invalid module","INVALID_MODULE");
}

void tools_sync_notifications(bool notify)
{
	NotificationTypes::GetInstance()->SyncBinaries(notify);
}

void tools_flush_retrier(void)
{
	Logger::Log(LOG_NOTICE,"Flushing retrier");
	Retrier *retrier = Retrier::GetInstance();
	retrier->Flush();
}

bool tools_handle_query(const User &user, XMLQuery *query, QueryResponse *response)
{
	const string action = query->GetRootAttribute("action");
	
	if(query->GetQueryGroup()=="control")
	{
		if(!user.IsAdmin())
			User::InsufficientRights();
		
		if(action=="reload")
		{
			string module = query->GetRootAttribute("module","");
			bool notify = query->GetRootAttributeBool("notify",true);
			if(module.length()==0)
				tools_config_reload("all",notify);
			else
				tools_config_reload(module,notify);
			
			return true;
		}
		else if(action=="retry")
		{
			tools_flush_retrier();
			return true;
		}
		else if(action=="syncnotifications")
		{
			bool notify = query->GetRootAttributeBool("notify",true);
			tools_sync_notifications(notify);
			return true;
		}
		else if(action=="shutdown")
		{
			// Send SIGTERM
			pid_t pid = getpid();
			kill(pid,SIGTERM);
			return true;
		}
	}
	else if(query->GetQueryGroup()=="status")
	{
		if(action=="query")
		{
			string type = query->GetRootAttribute("type");
			
			Statistics *stats = Statistics::GetInstance();
			
			if(type=="workflows")
			{
				stats->IncStatisticsQueries();
				
				int limit = query->GetRootAttributeInt("limit",0);
				
				WorkflowInstances *workflow_instances = WorkflowInstances::GetInstance();
				workflow_instances->SendStatus(user, response, limit);
				
				return true;
			}
			else if(type=="scheduler")
			{
				if(!user.IsAdmin())
					User::InsufficientRights();
				
				stats->IncStatisticsQueries();
				
				WorkflowScheduler *scheduler = WorkflowScheduler::GetInstance();
				scheduler->SendStatus(response);
				
				return true;
			}
			else if(type=="configuration")
			{
				if(!user.IsAdmin())
					User::InsufficientRights();
				
				stats->IncStatisticsQueries();
				
				ConfigurationEvQueue *config = ConfigurationEvQueue::GetInstance();
				config->SendConfiguration(response);
				
				return true;
			}
			else
				throw Exception("Status","Unknown type","UNKNOWN_TYPE");
		}
	}
	
	return false;
}
