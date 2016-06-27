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

#include <pthread.h>
#include <sys/socket.h>

WorkflowInstances *WorkflowInstances::instance = 0;

WorkflowInstances::WorkflowInstances()
{
	if(!instance)
		instance = this;
	
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
	
	// Check for waiters
	std::map<unsigned int,pthread_cond_t *>::iterator i;
	i = wi_wait.find(workflow_instance_id);
	if(i==wi_wait.end())
	{
		pthread_mutex_unlock(&lock);
		
		return;
	}
	
	// Release waiters
	pthread_cond_t *wait_cond = i->second;
	pthread_cond_broadcast(wait_cond);
	
	wi_wait.erase(workflow_instance_id);
	pthread_cond_destroy(wait_cond);
	delete wait_cond;
	
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

bool WorkflowInstances::Wait(unsigned int workflow_instance_id)
{
	pthread_mutex_lock(&lock);
	
	// First check specified instance exists
	std::map<unsigned int,WorkflowInstance *>::iterator i;
	i = wi.find(workflow_instance_id);
	if(i==wi.end())
	{
		pthread_mutex_unlock(&lock);
		
		return false;
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
	
	pthread_cond_wait(wait_cond, &lock);
	
	pthread_mutex_unlock(&lock);
	
	return true;
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

void WorkflowInstances::SendStatus(int s)
{
	pthread_mutex_lock(&lock);
	
	send(s,"<workflows>",11,0);
	
	for(std::map<unsigned int,WorkflowInstance *>::iterator i = wi.begin();i!=wi.end();++i)
		i->second->SendStatus(s,false);
	
	send(s,"</workflows>",12,0);
	
	pthread_mutex_unlock(&lock);
}

bool WorkflowInstances::SendStatus(int s,unsigned int workflow_instance_id)
{
	bool found = false;
	
	pthread_mutex_lock(&lock);
	
	std::map<unsigned int,WorkflowInstance *>::iterator i;
	i = wi.find(workflow_instance_id);
	if(i!=wi.end())
	{
		i->second->SendStatus(s,true);
		found = true;
	}
	
	pthread_mutex_unlock(&lock);
	
	return found;
}

void WorkflowInstances::RecordSavepoint()
{
	pthread_mutex_lock(&lock);
	
	for(std::map<unsigned int,WorkflowInstance *>::iterator i = wi.begin();i!=wi.end();++i)
	{
		i->second->RecordSavepoint();
		i->second->Shutdown();
		delete i->second;
	}
	
	pthread_mutex_unlock(&lock);
}
