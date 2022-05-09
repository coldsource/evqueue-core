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

#include <Queue/Queue.h>
#include <Queue/QueuePool.h>
#include <Logger/Logger.h>
#include <WorkflowInstance/WorkflowInstance.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <Logger/LoggerAPI.h>
#include <Exception/Exception.h>
#include <User/User.h>
#include <DB/DB.h>
#include <DOM/DOMDocument.h>
#include <WS/Events.h>
#include <API/QueryHandlers.h>

#include <string.h>
#include <stdio.h>
#include <ctype.h>

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	qh->RegisterHandler("queue",Queue::HandleQuery);
	Events::GetInstance()->RegisterEvents({"QUEUE_CREATED","QUEUE_MODIFIED","QUEUE_REMOVED"});
	return (APIAutoInit *)0;
});

using namespace std;

Queue::Queue(unsigned int id,const string &name, int concurrency, int scheduler, const string &wanted_scheduler, bool dynamic)
{
	if(!CheckQueueName(name))
		throw Exception("Queue","Invalid queue name '"+name+"'");
	
	this->id = id;
	this->name =name;
	this->concurrency = concurrency;
	this->scheduler = scheduler;
	this->wanted_scheduler = wanted_scheduler;
	this->dynamic = dynamic;
	
	size = 0;
	running_tasks = 0;
	
	removed = false;
}

Queue::~Queue()
{
	if(scheduler==QUEUE_SCHEDULER_FIFO)
	{
		for(auto it=queue.begin();it!=queue.end();it++)
			delete (*it);
	}
	else if(scheduler==QUEUE_SCHEDULER_PRIO)
	{
		for(auto it=prio_queue.begin();it!=prio_queue.end();it++)
			delete it->second;
	}
}

bool Queue::CheckQueueName(const string &queue_name)
{
	int i,len;
	
	len = queue_name.length();
	if(len==0 || len>QUEUE_NAME_MAX_LEN)
		return false;
	
	for(i=0;i<len;i++)
		if(!isalnum(queue_name[i]) && queue_name[i]!='_' && queue_name[i]!='-' && queue_name[i]!='@' && queue_name[i]!='.')
			return false;
	
	return true;
}

void Queue::EnqueueTask(WorkflowInstance *workflow_instance,DOMElement task)
{
	Task *new_task = new Task(workflow_instance,task);
	
	if(scheduler==QUEUE_SCHEDULER_FIFO)
		queue.push_back(new_task);
	else if(scheduler==QUEUE_SCHEDULER_PRIO)
		prio_queue.insert(std::pair<unsigned int,Task *>(workflow_instance->GetInstanceID(),new_task));
	
	size++;
}

bool Queue::DequeueTask(WorkflowInstance **p_workflow_instance,DOMElement *p_task)
{
	if(IsLocked())
		return false;
	
	Task *tmp;
	
	// If we have cancelled tasks, return it first
	if(cancelled_tasks.size())
	{
		// Dequeue cancelled task
		tmp = cancelled_tasks.front();
		cancelled_tasks.pop();
	}
	else
	{
		// Dequeue task
		if(scheduler==QUEUE_SCHEDULER_FIFO)
		{
			tmp = queue.front();
			queue.pop_front();
		}
		else if(scheduler==QUEUE_SCHEDULER_PRIO)
		{
			std::multimap<unsigned int,Task *>::iterator it;
			it=prio_queue.begin();
			tmp = it->second;
			prio_queue.erase(it);
		}
		
		size--;
	}
	
	// Retreive task data
	*p_workflow_instance = tmp->workflow_instance;
	*p_task = tmp->task;
	
	delete tmp;
	
	return true;
}

bool Queue::ExecuteTask(void)
{
	running_tasks++;
	return true;
}


bool Queue::TerminateTask(void)
{
	running_tasks--;
	return true;
}

bool Queue::CancelTasks(unsigned int workflow_instance_id)
{
	if(scheduler==QUEUE_SCHEDULER_FIFO)
	{
		for(auto it=queue.begin();it!=queue.end();)
		{
			if((*it)->workflow_instance->GetInstanceID()==workflow_instance_id)
			{
				cancelled_tasks.push((*it));
				it = queue.erase(it);
				
				size--;
			}
			else
				++it;
		}
	}
	else if(scheduler==QUEUE_SCHEDULER_PRIO)
	{
		for(auto it=prio_queue.cbegin();it!=prio_queue.cend();)
		{
			if(it->second->workflow_instance->GetInstanceID()==workflow_instance_id)
			{
				cancelled_tasks.push(it->second);
				prio_queue.erase(it++);
				
				size--;
			}
			else
				++it;
		}
	}
	
	return true;
}

void Queue::SetConcurrency(unsigned int concurrency)
{
	this->concurrency = concurrency;
	removed = false;
}

void Queue::SetDynamic(bool dynamic)
{
	this->dynamic = dynamic;
	removed = false;
}

void Queue::SetScheduler(unsigned int new_scheduler)
{
	if(new_scheduler==scheduler)
		return;
	
	Logger::Log(LOG_NOTICE,"[ Queue ] "+name+" : migrating scheduler to "+QueuePool::get_scheduler_from_int(new_scheduler));
	
	if(new_scheduler==QUEUE_SCHEDULER_FIFO)
	{
		for(auto it=prio_queue.begin();it!=prio_queue.end();it++)
			queue.push_back(it->second);
		prio_queue.clear();
	}
	
	if(new_scheduler==QUEUE_SCHEDULER_PRIO)
	{
		for(auto it=queue.begin();it!=queue.end();it++)
			prio_queue.insert(std::pair<unsigned int,Task *>((*it)->workflow_instance->GetInstanceID(),(*it)));
		queue.clear();
	}
	
	scheduler = new_scheduler;
}

bool Queue::IsLocked(void)
{
	if(cancelled_tasks.size())
		return false; // Always return cancelled tasks
	if(size>0 && running_tasks<concurrency)
		return false; // Return new tasks only if concurrency is not reached
	return true;
}

void Queue::Get(unsigned int id, QueryResponse *response)
{
	QueuePool::GetInstance()->GetQueue(id,response);
}

unsigned int Queue::Create(const string &name, int concurrency, const string &scheduler, int dynamic)
{
	create_edit_check(name,concurrency,scheduler);
	
	DB db;
	db.QueryPrintf("INSERT INTO t_queue(queue_name, queue_concurrency,queue_scheduler,queue_dynamic) VALUES(%s,%i,%s,%i)",&name,&concurrency,&scheduler,&dynamic);
	
	return db.InsertID();
}

void Queue::Edit(unsigned int id,const string &name, int concurrency, const string &scheduler, int dynamic)
{
	if(!QueuePool::GetInstance()->Exists(id))
		throw Exception("Queue","Unable to find queue","UNKNOWN_QUEUE");
	
	create_edit_check(name,concurrency,scheduler);
	
	DB db;
	db.QueryPrintf("UPDATE t_queue SET queue_name=%s, queue_concurrency=%i, queue_scheduler=%s, queue_dynamic=%i WHERE queue_id=%i",&name,&concurrency,&scheduler,&dynamic,&id);
}

void Queue::Delete(unsigned int id)
{
	DB db;
	db.QueryPrintf("DELETE FROM t_queue WHERE queue_id=%i",&id);
	if(!db.AffectedRows())
		throw Exception("Queue","Unable to find queue","UNKNOWN_QUEUE");
}

void Queue::create_edit_check(const std::string &name, int concurrency, const std::string &scheduler)
{
	if(!CheckQueueName(name))
		throw Exception("Queue","Invalid queue name","INVALID_PARAMETER");
	
	if(concurrency<=0)
		throw Exception("Queue","Invalid concurrency, must be greater than 0","INVALID_PARAMETER");
	
	if(scheduler!="prio" && scheduler!="fifo" && scheduler!="default")
		throw Exception("Queue","Invalid scheduler name, must be 'prio', 'fifo' or 'default'","INVALID_PARAMETER");
}

bool Queue::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	const string action = query->GetRootAttribute("action");
	
	if(action=="get")
	{
		unsigned int id = query->GetRootAttributeInt("id");
		
		Get(id,response);
		
		return true;
	}
	else if(action=="create" || action=="edit")
	{
		string name = query->GetRootAttribute("name");
		int iconcurrency = query->GetRootAttributeInt("concurrency");
		string scheduler = query->GetRootAttribute("scheduler");
		bool dynamic = query->GetRootAttributeBool("dynamic");
		
		if(action=="create")
		{
			unsigned int id = Create(name, iconcurrency, scheduler, dynamic);
			
			LoggerAPI::LogAction(user,id,"Queue",query->GetQueryGroup(),action);
			
			Events::GetInstance()->Create("QUEUE_CREATED");
			
			response->GetDOM()->getDocumentElement().setAttribute("queue-id",to_string(id));
		}
		else
		{
			unsigned int id = query->GetRootAttributeInt("id");
			
			Edit(id,name, iconcurrency, scheduler, dynamic);
			
			Events::GetInstance()->Create("QUEUE_MODIFIED");
			
			LoggerAPI::LogAction(user,id,"Queue",query->GetQueryGroup(),action);
		}
		
		QueuePool::GetInstance()->Reload();
		
		return true;
	}
	else if(action=="delete")
	{
		unsigned int id = query->GetRootAttributeInt("id");
		
		Delete(id);
		
		LoggerAPI::LogAction(user,id,"Queue",query->GetQueryGroup(),action);
		
		Events::GetInstance()->Create("QUEUE_REMOVED");
		
		QueuePool::GetInstance()->Reload();
		
		return true;
	}
	
	return false;
}
