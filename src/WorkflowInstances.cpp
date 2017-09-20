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

#include <WorkflowInstances.h>
#include <WorkflowInstance.h>
#include <WorkflowParameters.h>
#include <Statistics.h>
#include <Logger.h>
#include <Configuration.h>
#include <QueryResponse.h>
#include <SocketQuerySAX2Handler.h>
#include <Exception.h>
#include <DB.h>
#include <User.h>

#include <pthread.h>
#include <sys/socket.h>
#include <poll.h>

#include <string>
#include <vector>

WorkflowInstances *WorkflowInstances::instance = 0;

using namespace std;

WorkflowInstances::WorkflowInstances()
{
	if(!instance)
		instance = this;
	
	poll_interval = Configuration::GetInstance()->GetInt("dpd.interval");
	if(poll_interval<0)
		throw Exception("WorkflowInstances","DPD: Invalid poll interval, should be greater or equal than 0");
	
	is_shutting_down = false;
}

void WorkflowInstances::Add(unsigned int workflow_instance_id, WorkflowInstance *workflow_instance)
{
	unique_lock<mutex> llock(lock);
	
	wi[workflow_instance_id] = workflow_instance;
}

void WorkflowInstances::Remove(unsigned int workflow_instance_id)
{
	unique_lock<mutex> llock(lock);
	
	wi.erase(workflow_instance_id);
	
	release_waiters(workflow_instance_id);
}

bool WorkflowInstances::Cancel(const User &user, unsigned int workflow_instance_id)
{
	bool found = false;
	
	unique_lock<mutex> llock(lock);
	
	std::map<unsigned int,WorkflowInstance *>::iterator i;
	i = wi.find(workflow_instance_id);
	if(i!=wi.end())
	{
		if(!user.HasAccessToWorkflow(i->second->GetWorkflowID(), "kill"))
			User::InsufficientRights();
		
		(i->second)->Cancel();
		found = true;
	}
	
	return found;
}

bool WorkflowInstances::Wait(const User &user, QueryResponse *response, unsigned int workflow_instance_id, int timeout)
{
	if(timeout<0)
		return false;
	
	unique_lock<mutex> llock(lock);
	
	// First check specified instance exists
	std::map<unsigned int,WorkflowInstance *>::iterator i;
	i = wi.find(workflow_instance_id);
	if(i==wi.end())
		throw Exception("WorkflowInstances","Unknown instance ID");
	
	if(!user.HasAccessToWorkflow(i->second->GetWorkflowID(), "read"))
		User::InsufficientRights();
	
	condition_variable *wait_cond;
	
	// Check if a wait condition has already been set
	auto j = wi_wait.find(workflow_instance_id);
	if(j==wi_wait.end())
	{
		// No one is waiting, create wait condition
		wait_cond = new condition_variable;
		wi_wait[workflow_instance_id] = wait_cond;
	}
	else
		wait_cond = j->second; // Another thread is already waiting, wait together
	
	cv_status cond_re = cv_status::no_timeout,re;
	
	Statistics::GetInstance()->IncWaitingThreads();
	
	if(poll_interval==0)
	{
		// Dead peer detection is disabled
		
		if(timeout==0)
			wait_cond->wait(llock);
		else
		{
			auto abstime = chrono::system_clock::now() + chrono::seconds(timeout);
			cond_re = wait_cond->wait_until(llock,abstime);
		}
	}
	else
	{
		// Dead peer detection is enabled, use poll_interval as timeout
		
		auto abstime_end = chrono::system_clock::now();
		if(timeout!=0)
			abstime_end += chrono::seconds(timeout);
		
		while(1)
		{
			// Determine the amount of time we will wait
			auto abstime = chrono::system_clock::now();
			
			if(timeout==0)
				abstime += chrono::seconds(poll_interval);
			else
			{
				if(abstime+chrono::seconds(poll_interval)>abstime_end)
					abstime = abstime_end;
				else
					abstime += chrono::seconds(poll_interval);
			}
			
			cond_re = wait_cond->wait_until(llock,abstime);
			
			if(cond_re==cv_status::no_timeout)
				break; // Workflow has ended, normal exit condition
			else if(timeout!=0)
			{
				auto abstime = chrono::system_clock::now();
				if(abstime>abstime_end)
					break; // We have reached timeout
			}
			
			// Check if remote client is still alive
			if(!response->Ping())
			{
				Logger::Log(LOG_NOTICE,"Remote host is gone, terminating wait thread...");
				break;
			}
		}
	}
	
	Statistics::GetInstance()->DecWaitingThreads();
		
	if(is_shutting_down)
		return false; // Force error even if cond_re==0
	return (cond_re==cv_status::no_timeout);
}

bool WorkflowInstances::KillTask(const User &user, unsigned int workflow_instance_id, pid_t pid)
{
	bool found = false;
	
	unique_lock<mutex> llock(lock);
	
	std::map<unsigned int,WorkflowInstance *>::iterator i;
	i = wi.find(workflow_instance_id);
	if(i!=wi.end())
	{
		if(!user.HasAccessToWorkflow(i->second->GetWorkflowID(), "kill"))
			User::InsufficientRights();
		
		(i->second)->KillTask(pid);
		found = true;
	}
	
	return found;
}

void WorkflowInstances::SendStatus(const User &user, QueryResponse *response)
{
	unique_lock<mutex> llock(lock);
	
	for(std::map<unsigned int,WorkflowInstance *>::iterator i = wi.begin();i!=wi.end();++i)
	{
		if(!user.HasAccessToWorkflow(i->second->GetWorkflowID(), "read"))
			continue;
		
		i->second->SendStatus(response,false);
	}
}

bool WorkflowInstances::SendStatus(const User &user, QueryResponse *response,unsigned int workflow_instance_id)
{
	bool found = false;
	
	unique_lock<mutex> llock(lock);
	
	std::map<unsigned int,WorkflowInstance *>::iterator i;
	i = wi.find(workflow_instance_id);
	if(i!=wi.end())
	{
		if(!user.HasAccessToWorkflow(i->second->GetWorkflowID(), "read"))
			User::InsufficientRights();
		
		i->second->SendStatus(response,true);
		found = true;
	}
	
	return found;
}

void WorkflowInstances::RecordSavepoint()
{
	unique_lock<mutex> llock(lock);
	
	is_shutting_down = true;
	
	for(std::map<unsigned int,WorkflowInstance *>::iterator i = wi.begin();i!=wi.end();++i)
	{
		i->second->RecordSavepoint();
		i->second->Shutdown();
		
		release_waiters(i->second->GetInstanceID());
		
		delete i->second;
	}
}

bool WorkflowInstances::HandleQuery(const User &user, SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	const string action = saxh->GetRootAttribute("action");
	
	if(action=="list")
	{
		unsigned int filter_id = saxh->GetRootAttributeInt("filter_id",0);
		string filter_node = saxh->GetRootAttribute("filter_node","");
		string filter_workflow = saxh->GetRootAttribute("filter_workflow","");
		string filter_launched_from = saxh->GetRootAttribute("filter_launched_from","");
		string filter_launched_until = saxh->GetRootAttribute("filter_launched_until","");
		string filter_status = saxh->GetRootAttribute("filter_status","");
		bool filter_error = saxh->GetRootAttributeBool("filter_error",false);
		unsigned int filter_schedule_id = saxh->GetRootAttributeInt("filter_schedule_id",0);
		unsigned int limit = saxh->GetRootAttributeInt("limit",30);
		unsigned int offset = saxh->GetRootAttributeInt("offset",0);
		
		// Build query parts
		string query_select = "SELECT SQL_CALC_FOUND_ROWS wi.workflow_instance_id, w.workflow_name, wi.node_name, wi.workflow_instance_host, wi.workflow_instance_start, wi.workflow_instance_end, wi.workflow_instance_errors, wi.workflow_instance_status, wi.workflow_schedule_id, wi.workflow_instance_comment";
		string query_from = "FROM t_workflow_instance wi, t_workflow w";
		
		string query_where = "WHERE wi.workflow_id=w.workflow_id";
		vector<void *> query_where_values;
		
		if(filter_id!=0)
		{
			query_where += " AND wi.workflow_instance_id=%i";
			query_where_values.push_back(&filter_id);
		}
		
		if(filter_node.length())
		{
			query_where += " AND wi.node_name=%s";
			query_where_values.push_back(&filter_node);
		}
		
		if(filter_workflow.length())
		{
			query_where += " AND w.workflow_name=%s";
			query_where_values.push_back(&filter_workflow);
		}
		
		if(filter_launched_from.length())
		{
			query_where += " AND wi.workflow_instance_start>=%s";
			query_where_values.push_back(&filter_launched_from);
		}
		
		if(filter_launched_until.length())
		{
			query_where += " AND wi.workflow_instance_start<=%s";
			query_where_values.push_back(&filter_launched_until);
		}
		
		if(filter_status.length())
		{
			query_where += " AND wi.workflow_instance_status=%s";
			query_where_values.push_back(&filter_status);
		}
		
		if(filter_error)
			query_where += " AND wi.workflow_instance_errors>0";
		
		if(filter_schedule_id!=0)
		{
			query_where += " AND wi.workflow_schedule_id=%i";
			query_where_values.push_back(&filter_schedule_id);
		}
		
		if(!user.IsAdmin())
		{
			vector<int> read_workflows = user.GetReadAccessWorkflows();
			if(read_workflows.size()==0)
				query_where += " AND false";
			else
			{
				query_where += " AND w.workflow_id IN(";
				for(int i=0;i<read_workflows.size();i++)
				{
					if(i>0)
						query_where+= ", ";
					
					query_where += to_string(read_workflows.at(i));
				}
				
				query_where += ")";
			}
		}
		
		WorkflowParameters *parameters = saxh->GetWorkflowParameters();
		string *name, *value;
		
		parameters->SeekStart();
		while(parameters->Get(&name,&value))
		{
			query_where += " AND EXISTS(SELECT * FROM t_workflow_instance_parameters wip WHERE wip.workflow_instance_id=wi.workflow_instance_id AND workflow_instance_parameter=%s AND workflow_instance_parameter_value=%s)";
			query_where_values.push_back(name);
			query_where_values.push_back(value);
		}
		
		string query_order_by = "ORDER BY wi.workflow_instance_end DESC";
		
		string query_limit = "LIMIT "+to_string(offset)+","+to_string(limit);
		
		string query = query_select+" "+query_from+" "+query_where+" "+query_order_by+" "+query_limit;
		
		DB db;
		db.QueryVsPrintf(query,query_where_values);
		while(db.FetchRow())
		{
			DOMElement node = (DOMElement)response->AppendXML("<workflow />");
			node.setAttribute("id",to_string(db.GetFieldInt(0)));
			node.setAttribute("name",db.GetField(1));
			node.setAttribute("node_name",db.GetField(2));
			if(!db.GetFieldIsNULL(3))
				node.setAttribute("host",db.GetField(3));
			node.setAttribute("start_time",db.GetField(4));
			node.setAttribute("end_time",db.GetField(5));
			node.setAttribute("errors",db.GetField(6));
			node.setAttribute("status",db.GetField(7));
			if(!db.GetFieldIsNULL(8))
				node.setAttribute("schedule_id",db.GetField(8));
			node.setAttribute("comment",db.GetField(9));
		}
		
		db.Query("SELECT FOUND_ROWS()");
		db.FetchRow();
		response->GetDOM()->getDocumentElement().setAttribute("rows",db.GetField(0));
		
		return true;
	}
	
	return false;
}

void WorkflowInstances::release_waiters(unsigned int workflow_instance_id)
{
	// Check for waiters
	auto i = wi_wait.find(workflow_instance_id);
	if(i==wi_wait.end())
		return;
	
	// Release waiters
	i->second->notify_all();
	
	wi_wait.erase(workflow_instance_id);
	delete i->second;
}
