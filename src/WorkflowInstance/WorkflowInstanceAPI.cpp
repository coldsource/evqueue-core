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

#include <WorkflowInstance/WorkflowInstanceAPI.h>
#include <WorkflowInstance/WorkflowInstance.h>
#include <Workflow/Workflows.h>
#include <Workflow/Workflow.h>
#include <WorkflowInstance/WorkflowInstances.h>
#include <API/SocketQuerySAX2Handler.h>
#include <DB/SequenceGenerator.h>
#include <API/QueryResponse.h>
#include <API/Statistics.h>
#include <Exception/Exception.h>
#include <Logger/Logger.h>
#include <Schedule/Retrier.h>
#include <Queue/QueuePool.h>
#include <DB/DB.h>
#include <Configuration/ConfigurationEvQueue.h>
#include <User/User.h>
#include <Tag/Tag.h>
#include <WS/Events.h>

#include <string>
#include <map>

using namespace std;

void WorkflowInstanceAPI::Delete(unsigned int id)
{
	DB db;
	
	db.QueryPrintf("DELETE FROM t_workflow_instance WHERE workflow_instance_id=%i AND workflow_instance_status='TERMINATED'",&id);
	if(db.AffectedRows()==0)
		throw Exception("WorkflowInstanceAPI", "Instance ID not found","UNKNOWN_INSTANCE");
	
	db.QueryPrintf("DELETE FROM t_workflow_instance_parameters WHERE workflow_instance_id=%i",&id);
	
	db.QueryPrintf("DELETE FROM t_workflow_instance_filters WHERE workflow_instance_id=%i",&id);
	
	db.QueryPrintf("DELETE FROM t_datastore WHERE workflow_instance_id=%i",&id);
	
	db.QueryPrintf("DELETE FROM t_workflow_instance_tag WHERE workflow_instance_id=%i",&id);
}

void WorkflowInstanceAPI::Tag(unsigned int id, unsigned int tag_id)
{
	DB db;
	
	// Check tag existence
	::Tag tag(&db,tag_id);
	
	// Check instance existence
	db.QueryPrintf("SELECT workflow_instance_status FROM t_workflow_instance WHERE workflow_instance_id=%i",&id);
	if(!db.FetchRow())
		throw Exception("WorkflowInstanceAPI", "Instance ID not found","UNKNOWN_INSTANCE");
	
	if(db.GetField(0)!="TERMINATED")
		throw Exception("WorkflowInstanceAPI", "Instance is not terminated","INSTANCE_IS_RUNNING");
	
	db.QueryPrintf("SELECT COUNT(*) FROM t_workflow_instance_tag WHERE workflow_instance_id=%i AND tag_id=%i",&id,&tag_id);
	db.FetchRow();
	if(db.GetFieldInt(0)==1)
		throw Exception("WorkflowInstanceAPI", "Instance is already tagged","INSTANCE_ALREADY_TAGGED");
	
	db.QueryPrintf("INSERT INTO t_workflow_instance_tag(workflow_instance_id,tag_id) VALUES(%i,%i)",&id,&tag_id);
}

void WorkflowInstanceAPI::Untag(unsigned int id, unsigned int tag_id)
{
	DB db;
	
	db.QueryPrintf("DELETE FROM t_workflow_instance_tag WHERE workflow_instance_id=%i AND tag_id=%i",&id,&tag_id);
	if(db.AffectedRows()==0)
		throw Exception("WorkflowInstanceAPI", "Instance ID or tag ID not found","UNKNOWN_TAG_OR_INSTANCE");
}

bool WorkflowInstanceAPI::HandleQuery(const User &user, SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	const string action = saxh->GetRootAttribute("action");
	
	Statistics *stats = Statistics::GetInstance();
	Configuration *config = ConfigurationEvQueue::GetInstance();
	
	if(action=="launch")
	{
		string name = saxh->GetRootAttribute("name");
		
		Workflow workflow = Workflows::GetInstance()->Get(name);
		if(!user.HasAccessToWorkflow(workflow.GetID(), "exec"))
			User::InsufficientRights();
		
		string username = saxh->GetRootAttribute("user","");
		string host = saxh->GetRootAttribute("host","");
		string mode = saxh->GetRootAttribute("mode","asynchronous");
		string comment = saxh->GetRootAttribute("comment","");
		
		if(mode!="synchronous" && mode!="asynchronous")
			throw Exception("WorkflowInstance","mode must be 'synchronous' or 'asynchronous'","INVALID_PARAMETER");
		
		int timeout = saxh->GetRootAttributeInt("timeout",0);
		
		WorkflowInstance *wi;
		bool workflow_terminated;
		
		stats->IncWorkflowQueries();
		
		try
		{
			wi = new WorkflowInstance(name,saxh->GetWorkflowParameters(),0,host,username,comment);
		}
		catch(Exception &e)
		{
			stats->IncWorkflowExceptions();
			throw e;
		}
		
		unsigned int instance_id = wi->GetInstanceID();
		
		wi->Start(&workflow_terminated);
		
		Logger::Log(LOG_NOTICE,"[WID %d] Instantiated",wi->GetInstanceID());
		
		int wait_re = true;
		if(!workflow_terminated && mode=="synchronous")
			wait_re = WorkflowInstances::GetInstance()->Wait(user, response,instance_id,timeout);
		
		response->GetDOM()->getDocumentElement().setAttribute("workflow-instance-id",to_string(instance_id));
		if(!wait_re)
			response->GetDOM()->getDocumentElement().setAttribute("wait","timedout");
		
		if(workflow_terminated)
			delete wi; // This can happen on empty workflows or when dynamic errors occur in workflow (eg unknown queue for a task)
		
		return true;
	}
	else
	{
		unsigned int workflow_instance_id = saxh->GetRootAttributeInt("id");
		
		if(action=="debugresume")
		{
			DB db;
			
			// Fetch existing instance (must be terminated)
			db.QueryPrintf("SELECT workflow_id,workflow_instance_host,workflow_instance_savepoint,workflow_instance_comment FROM t_workflow_instance WHERE workflow_instance_id=%i AND workflow_instance_status='TERMINATED'",&workflow_instance_id);
			if(!db.FetchRow())
				throw Exception("Workflow Debug Resume", "Unable to find instance to resume, or instance is not terminated");
			
			unsigned int workflow_id = db.GetFieldInt(0);
			if(!user.HasAccessToWorkflow(workflow_id, "read") || !user.HasAccessToWorkflow(workflow_id, "exec"))
					User::InsufficientRights();
			
			int savepoint_level = ConfigurationEvQueue::GetInstance()->GetInt("workflowinstance.savepoint.level");
			
			// Clone existing instance
			string node_name = ConfigurationEvQueue::GetInstance()->Get("cluster.node.name");
			string workflow_host = db.GetField(1);
			string savepoint = db.GetField(2);
			string instance_comment = db.GetField(3);
			int new_instance_id = 0;
			if(savepoint_level<2)
				SequenceGenerator::GetInstance()->GetInc();
			db.QueryPrintf("INSERT INTO t_workflow_instance(workflow_instance_id,node_name,workflow_id,workflow_schedule_id,workflow_instance_host,workflow_instance_status,workflow_instance_start,workflow_instance_comment,workflow_instance_savepoint) VALUES(%i,%s,%i,0,%s,'EXECUTING',NOW(),%s,%s)",savepoint_level<2?new_instance_id:0,&node_name,&workflow_id,&workflow_host,&instance_comment,&savepoint);
			
			if(savepoint_level>=2)
				new_instance_id = db.InsertID();
			
			// Clone parameters if needed
			if(ConfigurationEvQueue::GetInstance()->GetBool("workflowinstance.saveparameters"))
				db.QueryPrintf("INSERT INTO t_workflow_instance_parameters(workflow_instance_id,workflow_instance_parameter,workflow_instance_parameter_value) SELECT %i,workflow_instance_parameter,workflow_instance_parameter_value FROM t_workflow_instance_parameters WHERE workflow_instance_id=%i",&new_instance_id,&workflow_instance_id);
			
			WorkflowInstance *wi;
			bool workflow_terminated;
			
			try
			{
				wi = new WorkflowInstance(new_instance_id);
			}
			catch(Exception &e)
			{
				stats->IncWorkflowExceptions();
				throw e;
			}
			
			wi->DebugResume(&workflow_terminated);
			
			Logger::Log(LOG_NOTICE,"[WID %d] Resumed in debug mode",wi->GetInstanceID());
			
			response->GetDOM()->getDocumentElement().setAttribute("workflow-instance-id",to_string(new_instance_id));
			
			if(workflow_terminated)
				delete wi; // This can happen if nothing is to resume
			
			return true;
		}
		if(action=="migrate")
		{
			if(!user.IsAdmin())
				User::InsufficientRights();
			
			DB db;
			
			// Check if workflow instance exists and is eligible to migration
			db.QueryPrintf("SELECT workflow_instance_id FROM t_workflow_instance WHERE workflow_instance_id=%i AND workflow_instance_status='EXECUTING' AND node_name!=%s",&workflow_instance_id,&config->Get("cluster.node.name"));
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
			
			if(!wfi->SendStatus(user, response,workflow_instance_id))
			{
				// Workflow is not executing, lookup in database
				db.QueryPrintf("SELECT workflow_instance_savepoint, workflow_id, workflow_instance_status FROM t_workflow_instance WHERE workflow_instance_id=%i",&workflow_instance_id);
				if(!db.FetchRow())
					throw Exception("WorkflowInstance","Unknown workflow instance","UNKNOWN_INSTANCE");
				
				if(!user.HasAccessToWorkflow(db.GetFieldInt(1), "read"))
					User::InsufficientRights();
				
				if(string(db.GetField(2))!="TERMINATED")
					throw Exception("WorkflowInstance","Workflow instance is still running on another node, query the corresponding node","WRONG_NODE");
				
				if(db.GetFieldIsNULL(0))
					throw Exception("WorkflowInstance","Invalid workflow XML","INVALID_XML");
				
				response->AppendXML(db.GetField(0));
			}
			
			return true;
		}
		else if(action=="cancel")
		{
			stats->IncWorkflowCancelQueries();
			
			// Prevent workflow instance from instanciating new tasks
			if(!WorkflowInstances::GetInstance()->Cancel(user, workflow_instance_id))
				throw Exception("WorkflowInstance","Unknown workflow instance","UNKNOWN_INSTANCE");
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
			
			if(!WorkflowInstances::GetInstance()->Wait(user, response,workflow_instance_id,timeout))
				throw Exception("WorkflowInstance","Wait timed out","TIMED_OUT");
			
			return true;
		}
		else if(action=="killtask")
		{
			unsigned int task_pid = saxh->GetRootAttributeInt("pid");
			
			if(!WorkflowInstances::GetInstance()->KillTask(user, workflow_instance_id,task_pid))
				throw Exception("WorkflowInstance","Unknown workflow instance","UNKNOWN_INSTANCE");
			
			return true;
		}
		else if(action=="delete")
		{
			if(!user.IsAdmin())
				User::InsufficientRights();
			
			Delete(workflow_instance_id);
			
			Events::GetInstance()->Create(Events::en_types::INSTANCE_REMOVED,workflow_instance_id);
			
			return true;
		}
		else if(action=="tag")
		{
			if(!user.HasAccessToWorkflow(workflow_instance_id, "exec") && !user.HasAccessToWorkflow(workflow_instance_id, "edit"))
				User::InsufficientRights();
			
			unsigned int tag_id = saxh->GetRootAttributeInt("tag_id");
			
			Tag(workflow_instance_id,tag_id);
			
			return true;
		}
		else if(action=="untag")
		{
			if(!user.HasAccessToWorkflow(workflow_instance_id, "exec") && !user.HasAccessToWorkflow(workflow_instance_id, "edit"))
				User::InsufficientRights();
			
			unsigned int tag_id = saxh->GetRootAttributeInt("tag_id");
			
			Untag(workflow_instance_id,tag_id);
			
			return true;
		}
	}
	
	return false;
}
