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
#include <ELogs/Channels.h>
#include <ELogs/ChannelGroups.h>
#include <Exception/Exception.h>
#include <API/QueryHandlers.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

using namespace std;

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	qh->RegisterHandler("control",tools_handle_query);
	qh->RegisterHandler("status",tools_handle_query);
	return (APIAutoInit *)0;
});

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
			string module = query->GetRootAttribute("module","all");
			bool notify = query->GetRootAttributeBool("notify",true);
			
			if(!QueryHandlers::GetInstance()->Reload(module, notify))
				throw Exception("Control","Invalid module : "+module,"INVALID_MODULE");
			
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
