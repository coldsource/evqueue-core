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

#include <pthread.h>
#include <sys/socket.h>
#include <poll.h>

#include <xqilla/xqilla-dom3.hpp>

#include <string>
#include <vector>

WorkflowInstances *WorkflowInstances::instance = 0;

using namespace std;
using namespace xercesc;

WorkflowInstances::WorkflowInstances()
{
	if(!instance)
		instance = this;
	
	poll_interval = Configuration::GetInstance()->GetInt("dpd.interval");
	if(poll_interval<0)
		throw Exception("WorkflowInstances","DPD: Invalid poll interval, should be greater or equal than 0");
	
	is_shutting_down = false;
	
	pthread_mutex_init(&lock, NULL);
}

void WorkflowInstances::Add(unsigned int workflow_instance_id, WorkflowInstance *workflow_instance)
{
	pthread_mutex_lock(&lock);
	
	wi[workflow_instance_id] = workflow_instance;
	
	pthread_mutex_unlock(&lock);
}

void WorkflowInstances::Remove(unsigned int workflow_instance_id)
{
	pthread_mutex_lock(&lock);
	
	wi.erase(workflow_instance_id);
	
	release_waiters(workflow_instance_id);
	
	pthread_mutex_unlock(&lock);
}

bool WorkflowInstances::Cancel(unsigned int workflow_instance_id)
{
	bool found = false;
	
	pthread_mutex_lock(&lock);
	
	std::map<unsigned int,WorkflowInstance *>::iterator i;
	i = wi.find(workflow_instance_id);
	if(i!=wi.end())
	{
		(i->second)->Cancel();
		found = true;
	}
	
	pthread_mutex_unlock(&lock);
	
	return found;
}

bool WorkflowInstances::Wait(QueryResponse *response, unsigned int workflow_instance_id, int timeout)
{
	if(timeout<0)
		return false;
	
	pthread_mutex_lock(&lock);
	
	// First check specified instance exists
	std::map<unsigned int,WorkflowInstance *>::iterator i;
	i = wi.find(workflow_instance_id);
	if(i==wi.end())
	{
		pthread_mutex_unlock(&lock);
		
		throw Exception("WorkflowInstances","Unknown instance ID");
	}
	
	pthread_cond_t *wait_cond;
	
	// Check if a wait condition has already been set
	std::map<unsigned int,pthread_cond_t *>::iterator j;
	j = wi_wait.find(workflow_instance_id);
	if(j==wi_wait.end())
	{
		// No one is waiting, create wait condition
		wait_cond = new pthread_cond_t;
		pthread_cond_init(wait_cond, NULL);
		wi_wait[workflow_instance_id] = wait_cond;
	}
	else
		wait_cond = j->second; // Another thread is already waiting, wait together
	
	int cond_re,re;
	
	Statistics::GetInstance()->IncWaitingThreads();
	
	if(poll_interval==0)
	{
		// Dead peer detection is disabled
		
		if(timeout==0)
			cond_re = pthread_cond_wait(wait_cond, &lock);
		else
		{
			timespec abstime;
			clock_gettime(CLOCK_REALTIME,&abstime);
			abstime.tv_sec += timeout;
			cond_re = pthread_cond_timedwait(wait_cond,&lock,&abstime);
		}
	}
	else
	{
		// Dead peer detection is enabled, use poll_interval as timeout
		
		timespec abstime_end;
		if(timeout!=0)
		{
			clock_gettime(CLOCK_REALTIME,&abstime_end);
			abstime_end.tv_sec += timeout;
		}
		
		while(1)
		{
			// Determine the amount of time we will wait
			timespec abstime;
			clock_gettime(CLOCK_REALTIME,&abstime);
			
			if(timeout==0)
				abstime.tv_sec += poll_interval;
			else
			{
				if(abstime.tv_sec+poll_interval>abstime_end.tv_sec || (abstime.tv_sec+poll_interval==abstime_end.tv_sec && abstime.tv_nsec>abstime_end.tv_nsec))
					abstime = abstime_end;
				else
					abstime.tv_sec += poll_interval;
			}
			
			cond_re = pthread_cond_timedwait(wait_cond,&lock,&abstime);
			
			if(cond_re==0)
				break; // Workflow has ended, normal exit condition
			else if(timeout!=0)
			{
				clock_gettime(CLOCK_REALTIME,&abstime);
				if(abstime.tv_sec>abstime_end.tv_sec || (abstime.tv_sec==abstime_end.tv_sec && abstime.tv_nsec>=abstime_end.tv_nsec))
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
		
	pthread_mutex_unlock(&lock);
	
	if(is_shutting_down)
		return false; // Force error even if cond_re==0
	return (cond_re==0);
}

bool WorkflowInstances::KillTask(unsigned int workflow_instance_id, pid_t pid)
{
	bool found = false;
	
	pthread_mutex_lock(&lock);
	
	std::map<unsigned int,WorkflowInstance *>::iterator i;
	i = wi.find(workflow_instance_id);
	if(i!=wi.end())
	{
		(i->second)->KillTask(pid);
		found = true;
	}
	
	pthread_mutex_unlock(&lock);
	
	return found;
}

void WorkflowInstances::SendStatus(QueryResponse *response)
{
	pthread_mutex_lock(&lock);
	
	for(std::map<unsigned int,WorkflowInstance *>::iterator i = wi.begin();i!=wi.end();++i)
		i->second->SendStatus(response,false);
	
	pthread_mutex_unlock(&lock);
}

bool WorkflowInstances::SendStatus(QueryResponse *response,unsigned int workflow_instance_id)
{
	bool found = false;
	
	pthread_mutex_lock(&lock);
	
	std::map<unsigned int,WorkflowInstance *>::iterator i;
	i = wi.find(workflow_instance_id);
	if(i!=wi.end())
	{
		i->second->SendStatus(response,true);
		found = true;
	}
	
	pthread_mutex_unlock(&lock);
	
	return found;
}

void WorkflowInstances::RecordSavepoint()
{
	pthread_mutex_lock(&lock);
	
	is_shutting_down = true;
	
	for(std::map<unsigned int,WorkflowInstance *>::iterator i = wi.begin();i!=wi.end();++i)
	{
		i->second->RecordSavepoint();
		i->second->Shutdown();
		
		release_waiters(i->second->GetInstanceID());
		
		delete i->second;
	}
	
	pthread_mutex_unlock(&lock);
}

bool WorkflowInstances::HandleQuery(SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	const string action = saxh->GetRootAttribute("action");
	
	if(action=="list")
	{
		string filter_node = saxh->GetRootAttribute("filter_node","");
		string filter_workflow = saxh->GetRootAttribute("filter_workflow","");
		string filter_launched_from = saxh->GetRootAttribute("filter_launched_from","");
		string filter_launched_until = saxh->GetRootAttribute("filter_launched_until","");
		unsigned int limit = saxh->GetRootAttributeInt("limit",30);
		unsigned int offset = saxh->GetRootAttributeInt("offset",0);
		
		// Build query parts
		string query_select = "SELECT wi.workflow_instance_id, w.workflow_name, wi.node_name, wi.workflow_instance_host, wi.workflow_instance_start, wi.workflow_instance_end, wi.workflow_instance_errors, wi.workflow_instance_status";
		string query_from = "FROM t_workflow_instance wi, t_workflow w";
		
		string query_where = "WHERE true";
		vector<void *> query_where_values;
		
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
		
		WorkflowParameters *parameters = saxh->GetWorkflowParameters();
		string *name, *value;
		
		parameters->SeekStart();
		while(parameters->Get(&name,&value))
		{
			query_where += " AND EXISTS(SELECT * FROM t_workflow_instance_parameters wip WHERE wip.workflow_instance_id=wi.workflow_instance_id AND workflow_instance_parameter=%s AND workflow_instance_parameter_value=%s)";
			query_where_values.push_back(name);
			query_where_values.push_back(value);
		}
		
		string query_limit = "limit "+to_string(offset)+","+to_string(limit);
		
		string query = query_select+" "+query_from+" "+query_where+" "+query_limit;
		
		DB db;
		db.QueryVsPrintf(query,query_where_values);
		while(db.FetchRow())
		{
			DOMElement *node = (DOMElement *)response->AppendXML("<instance />");
			node->setAttribute(X("id"),X(std::to_string(db.GetFieldInt(0)).c_str()));
			node->setAttribute(X("name"),X(db.GetField(1)));
			node->setAttribute(X("node_name"),X(db.GetField(2)));
			node->setAttribute(X("host"),X(db.GetField(3)));
			node->setAttribute(X("start_time"),X(db.GetField(4)));
			node->setAttribute(X("end_time"),X(db.GetField(5)));
			node->setAttribute(X("errors"),X(db.GetField(6)));
			node->setAttribute(X("status"),X(db.GetField(7)));
		}
		
		return true;
	}
	
	return false;
}

void WorkflowInstances::release_waiters(unsigned int workflow_instance_id)
{
	// Check for waiters
	std::map<unsigned int,pthread_cond_t *>::iterator i;
	i = wi_wait.find(workflow_instance_id);
	if(i==wi_wait.end())
		return;
	
	// Release waiters
	pthread_cond_t *wait_cond = i->second;
	pthread_cond_broadcast(wait_cond);
	
	wi_wait.erase(workflow_instance_id);
	pthread_cond_destroy(wait_cond);
	delete wait_cond;
}
