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

#include <WorkflowInstanceAPI.h>
#include <WorkflowInstance.h>
#include <WorkflowInstances.h>
#include <SocketQuerySAX2Handler.h>
#include <QueryResponse.h>
#include <Statistics.h>
#include <Exception.h>
#include <Logger.h>
#include <Retrier.h>
#include <QueuePool.h>
#include <DB.h>
#include <Configuration.h>

#include <string>
#include <map>

#include <xqilla/xqilla-dom3.hpp>

using namespace std;
using namespace xercesc;

bool WorkflowInstanceAPI::HandleQuery(SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	const string action = saxh->GetRootAttribute("action");
	
	Statistics *stats = Statistics::GetInstance();
	Configuration *config = Configuration::GetInstance();
	
	if(action=="launch")
	{
		string name = saxh->GetRootAttribute("name");
		string user = saxh->GetRootAttribute("user","");
		string host = saxh->GetRootAttribute("host","");
		string mode = saxh->GetRootAttribute("mode","asynchronous");
		
		if(mode!="synchronous" && mode!="asynchronous")
			throw Exception("WorkflowInstance","mode must be 'synchronous' or 'asynchronous'");
		
		int timeout = saxh->GetRootAttributeInt("timeout",0);
		
		WorkflowInstance *wi;
		bool workflow_terminated;
		
		stats->IncWorkflowQueries();
		
		try
		{
			wi = new WorkflowInstance(name.c_str(),saxh->GetWorkflowParameters(),0,host.length()?host.c_str():0,user.length()?user.c_str():0);
		}
		catch(Exception &e)
		{
			stats->IncWorkflowExceptions();
			throw e;
		}
		
		unsigned int instance_id = wi->GetInstanceID();
		
		wi->Start(&workflow_terminated);
		
		Logger::Log(LOG_NOTICE,"[WID %d] Instanciated",wi->GetInstanceID());
		
		int wait_re = true;
		if(mode=="synchronous")
			wait_re = WorkflowInstances::GetInstance()->Wait(response,instance_id,timeout);
		
		response->GetDOM()->getDocumentElement()->setAttribute(X("workflow-instance-id"),X(to_string(instance_id).c_str()));
		if(!wait_re)
			response->GetDOM()->getDocumentElement()->setAttribute(X("wait"),X("timedout"));
		
		if(workflow_terminated)
			delete wi; // This can happen on empty workflows or when dynamic errors occur in workflow (eg unknown queue for a task)
		
		return true;
	}
	else
	{
		unsigned int workflow_instance_id = saxh->GetRootAttributeInt("id");
		
		if(action=="migrate")
		{
			DB db;
			
			// Check if workflow instance exists and is eligible to migration
			db.QueryPrintf("SELECT workflow_instance_id FROM t_workflow_instance WHERE workflow_instance_id=%i AND workflow_instance_status='EXECUTING' AND node_name!=%s",&workflow_instance_id,config->Get("network.node.name").c_str());
			if(!db.FetchRow())
				throw Exception("Workflow Migration","Workflow ID not found or already belongs to this node");
			
			Logger::Log(LOG_NOTICE,"[WID %d] Migrating",db.GetFieldInt(0));
			
			WorkflowInstance *workflow_instance = 0;
			bool workflow_terminated;
			try
			{
				workflow_instance = new WorkflowInstance(db.GetFieldInt(0));
				workflow_instance->Migrate(&workflow_terminated);
				if(workflow_terminated)
					delete workflow_instance;
			}
			catch(Exception &e)
			{
				if(workflow_instance)
					delete workflow_instance;
				throw e;
			}
			
			return true;
		}
		else if(action=="query")
		{
			DB db;
			
			stats->IncWorkflowStatusQueries();
			
			WorkflowInstances *wfi = WorkflowInstances::GetInstance();
			if(!wfi->SendStatus(response,workflow_instance_id))
			{
				// Workflow is not executing, lookup in database
				db.QueryPrintf("SELECT workflow_instance_savepoint FROM t_workflow_instance WHERE workflow_instance_id=%i",&workflow_instance_id);
				if(!db.FetchRow())
					throw Exception("WorkflowInstance","Unknown workflow instance");
				
				response->AppendXML(db.GetField(0));
			}
			
			return true;
		}
		else if(action=="cancel")
		{
			stats->IncWorkflowCancelQueries();
			
			// Prevent workflow instance from instanciating new tasks
			if(!WorkflowInstances::GetInstance()->Cancel(workflow_instance_id))
				throw Exception("WorkflowInstance","Unknown workflow instance");
			else
			{
				// Flush retrier
				Retrier::GetInstance()->FlushWorkflowInstance(workflow_instance_id);
				
				// Cancel currently queued tasks
				QueuePool::GetInstance()->CancelTasks(workflow_instance_id);
			}
			
			return true;
		}
		else if(action=="wait")
		{
			int timeout = saxh->GetRootAttributeInt("timeout",0);
			
			if(!WorkflowInstances::GetInstance()->Wait(response,workflow_instance_id,timeout))
				throw Exception("WorkflowInstance","Wait timed out");
			
			return true;
		}
		else if(action=="killtask")
		{
			unsigned int task_pid = saxh->GetRootAttributeInt("pid");
			
			if(!WorkflowInstances::GetInstance()->KillTask(workflow_instance_id,task_pid))
				throw Exception("WorkflowInstance","Unknown workflow instance");
			
			return true;
		}
	}
	
	return false;
}