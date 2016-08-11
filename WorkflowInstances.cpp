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
#include <Statistics.h>
#include <Logger.h>
#include <Configuration.h>
#include <QueryResponse.h>
#include <Exception.h>

#include <pthread.h>
#include <sys/socket.h>
#include <poll.h>

WorkflowInstances *WorkflowInstances::instance = 0;

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
