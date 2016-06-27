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

#include <WorkflowScheduler.h>
#include <WorkflowSchedule.h>
#include <WorkflowInstance.h>
#include <WorkflowParameters.h>
#include <Statistics.h>
#include <Exception.h>
#include <DB.h>
#include <Logger.h>
#include <Configuration.h>

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <xqilla/xqilla-dom3.hpp>
#include <xercesc/dom/DOM.hpp>

using namespace xercesc;

WorkflowScheduler *WorkflowScheduler::instance = 0;

WorkflowScheduler::WorkflowScheduler(): Scheduler()
{
	self_name = "Workflow scheduler";
	instance = this;
	
	pthread_mutexattr_t wfs_mutex_attr;
	pthread_mutexattr_init(&wfs_mutex_attr);
	pthread_mutexattr_settype(&wfs_mutex_attr,PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&wfs_mutex,&wfs_mutex_attr);
	
	init();
}

WorkflowScheduler::~WorkflowScheduler()
{
	if(wfs_ids)
		delete[] wfs_ids;
	
	if(wfs_wi_ids)
		delete[] wfs_wi_ids;
	
	if(wfs_executing_instances)
		delete[] wfs_executing_instances;
	
	// Delete workflows schedules
	Event *tmp;
	Event *event = first_event;
	while(event)
	{
		tmp = event;
		event = event->next_event;
		
		ScheduledWorkflow *scheduled_wf = (ScheduledWorkflow *)tmp;
		delete scheduled_wf->workflow_schedule;
	}
}

void WorkflowScheduler::ScheduleWorkflow(WorkflowSchedule *workflow_schedule, unsigned int workflow_instance_id)
{
	pthread_mutex_lock(&wfs_mutex);
	
	if(!workflow_instance_id)
	{
		// Workflow is currently inactive, schedule it
		ScheduledWorkflow *new_scheduled_wf = new ScheduledWorkflow;
		new_scheduled_wf->workflow_schedule = workflow_schedule;
		new_scheduled_wf->scheduled_at = workflow_schedule->GetNextTime();
		
		char buf[32];
		struct tm time_t;
		localtime_r(&new_scheduled_wf->scheduled_at,&time_t);
		strftime(buf,32,"%Y-%m-%d %H:%M:%S",&time_t);
		Logger::Log(LOG_NOTICE,"[WSID %d] Scheduled workflow %s at %s",workflow_schedule->GetID(),workflow_schedule->GetWorkflowName(),buf);
		
		InsertEvent(new_scheduled_wf);
	}
	else
	{
		// This schedule has a running workflow instance, put it in executing instances
		int i = lookup_wfs(workflow_schedule->GetID());
		if(i!=-1)
		{
			wfs_wi_ids[i] = workflow_instance_id;
			wfs_executing_instances[i] = workflow_schedule;
		}
	}
	
	pthread_mutex_unlock(&wfs_mutex);
}

void WorkflowScheduler::ScheduledWorkflowInstanceStop(unsigned int workflow_schedule_id, bool success)
{
	pthread_mutex_lock(&wfs_mutex);
	
	int i = lookup_wfs(workflow_schedule_id);
	if(i==-1)
	{
		pthread_mutex_unlock(&wfs_mutex);
		
		Logger::Log(LOG_NOTICE,"[WSID %d] Scheduled workflow instance stopped but workflow schedule has vanished, ignoring...",workflow_schedule_id);
		return; // Can happen when schedule has been removed
	}
	
	WorkflowSchedule *workflow_schedule = wfs_executing_instances[i];
	wfs_wi_ids[i] = 0;
	wfs_executing_instances[i] = 0; // Remove from executing instances
	
	if(success || workflow_schedule->GetOnFailureBehavior()==CONTINUE) // Re-schedule
		ScheduleWorkflow(workflow_schedule);
	else
	{
		Logger::Log(LOG_NOTICE,"[WSID %d] Removing schedule due to workflow errors",workflow_schedule->GetID());
		workflow_schedule->SetStatus(false);
		delete workflow_schedule;
	}
	
	pthread_mutex_unlock(&wfs_mutex);
}

void WorkflowScheduler::event_removed(Event *e, event_reasons reason)
{
	ScheduledWorkflow *scheduled_wf = (ScheduledWorkflow *)e;
	
	if(reason==ALARM)
	{
		Statistics *stats = Statistics::GetInstance();
		
		const char *workflow_name = scheduled_wf->workflow_schedule->GetWorkflowName();
		bool workflow_terminated;
		
		WorkflowInstance *wi = 0;
		
		int i = lookup_wfs(scheduled_wf->workflow_schedule->GetID());
		
		try
		{
			// We have to fill this first because WorkflowInstance() can throw an exception and call ScheduledWorkflowInstanceStop() in it's destructor
			if(i!=-1)
				wfs_executing_instances[i] = scheduled_wf->workflow_schedule;
			
			wi = new WorkflowInstance(workflow_name,scheduled_wf->workflow_schedule->GetParameters(),scheduled_wf->workflow_schedule->GetID(),scheduled_wf->workflow_schedule->GetHost(),scheduled_wf->workflow_schedule->GetUser());
			
			// Put instance in executing list
			pthread_mutex_lock(&wfs_mutex);
			
			if(i!=-1)
				wfs_wi_ids[i] = wi->GetInstanceID();
			
			pthread_mutex_unlock(&wfs_mutex);
			
			Logger::Log(LOG_NOTICE,"[WID %d] Instanciated by workflow scheduler",wi->GetInstanceID());
			
			wi->Start(&workflow_terminated);
			
			if(workflow_terminated)
				delete wi; // This can happen on empty workflows or when dynamic errors occur in workflow (eg unknown queue for a task)
		}
		catch(Exception &e)
		{
			stats->IncWorkflowExceptions();
			
			Logger::Log(LOG_WARNING,"[ WorkflowScheduler ] Unexpected exception trying to instanciate workflow '%s': [ %s ] %s",workflow_name,e.context,e.error);
			delete scheduled_wf;
			
			if(wi)
				delete wi;
			
			return;
		}
	}
	else if(reason==FLUSH)
		delete scheduled_wf->workflow_schedule;
	
	delete scheduled_wf;
}

void WorkflowScheduler::Reload()
{
	Logger::Log(LOG_NOTICE,"[ WorkflowScheduler ] Reloading configuration from database");
	
	pthread_mutex_lock(&wfs_mutex);
	
	Flush();
	
	// Backup running instances for later
	unsigned int *backup_wfs_ids = wfs_ids;
	unsigned int *backup_wfs_wi_ids = wfs_wi_ids;
	WorkflowSchedule **backup_wfs_executing_instances = wfs_executing_instances;
	int backup_num_wfs = num_wfs;
	
	// Reinit from database since there can be created or deleted schedules
	init();
	
	DB db;
	
	db.QueryPrintf("SELECT workflow_schedule_id FROM t_workflow_schedule WHERE node_name=%s AND workflow_schedule_active = 1",Configuration::GetInstance()->Get("network.node.name").c_str());
	while(db.FetchRow())
	{
		WorkflowSchedule *workflow_schedule = 0;
		try
		{
			workflow_schedule = new WorkflowSchedule(db.GetFieldInt(0));
			
			int i = lookup(backup_wfs_ids,backup_num_wfs,db.GetFieldInt(0));
			if(i==-1 || backup_wfs_executing_instances[i]==0)
				ScheduleWorkflow(workflow_schedule);
			else
				ScheduleWorkflow(workflow_schedule,backup_wfs_wi_ids[i]);
		}
		catch(Exception &e)
		{
			Logger::Log(LOG_NOTICE,"[WSID %d] Unexpected exception trying initialize workflow schedule : [ %s ] %s\n",db.GetFieldInt(0),e.context,e.error);
			
			if(workflow_schedule)
				delete workflow_schedule;
		}
	}
	
	// Clean old schedules
	for(int i=0;i<backup_num_wfs;i++)
		if(backup_wfs_executing_instances[i])
			delete backup_wfs_executing_instances[i];
	
	// Clean old references
	delete[] backup_wfs_ids;
	delete[] backup_wfs_wi_ids;
	delete[] backup_wfs_executing_instances;
	
	pthread_mutex_unlock(&wfs_mutex);
}

void WorkflowScheduler::SendStatus(int s)
{
	char buf[32];
	
	DOMImplementation *xqillaImplementation = DOMImplementationRegistry::getDOMImplementation(X("XPath2 3.0"));
	DOMDocument *xmldoc = xqillaImplementation->createDocument();
	
	DOMElement *status_node = xmldoc->createElement(X("status"));
	xmldoc->appendChild(status_node);
	
	// We need to get all mutexes to guarantee that status dump is fully coherent
	pthread_mutex_lock(&wfs_mutex);
	pthread_mutex_lock(&mutex);
	
	ScheduledWorkflow *event = (ScheduledWorkflow *)first_event;
	while(event)
	{
		DOMElement *workflow_node = xmldoc->createElement(X("workflow"));
		
		sprintf(buf,"%d",event->workflow_schedule->GetID());
		workflow_node->setAttribute(X("workflow_schedule_id"),X(buf));
		
		workflow_node->setAttribute(X("name"),X(event->workflow_schedule->GetWorkflowName()));
		
		struct tm time_t;
		localtime_r(&event->scheduled_at,&time_t);
		strftime(buf,32,"%Y-%m-%d %H:%M:%S",&time_t);
		workflow_node->setAttribute(X("scheduled_at"),X(buf));
		
		status_node->appendChild(workflow_node);
		
		event = (ScheduledWorkflow *)event->next_event;
	}
	
	for(int i=0;i<num_wfs;i++)
	{
		if(wfs_executing_instances[i])
		{
			DOMElement *workflow_node = xmldoc->createElement(X("workflow"));
			
			sprintf(buf,"%d",wfs_executing_instances[i]->GetID());
			workflow_node->setAttribute(X("workflow_schedule_id"),X(buf));
			
			workflow_node->setAttribute(X("name"),X(wfs_executing_instances[i]->GetWorkflowName()));
			
			sprintf(buf,"%d",wfs_wi_ids[i]);
			workflow_node->setAttribute(X("workflow_instance_id"),X(buf));
			
			workflow_node->setAttribute(X("scheduled_at"),X("running"));
			
			status_node->appendChild(workflow_node);
		}
	}
	
	pthread_mutex_unlock(&mutex);
	pthread_mutex_unlock(&wfs_mutex);
	
	DOMLSSerializer *serializer = xqillaImplementation->createLSSerializer();
	XMLCh *status_xml = serializer->writeToString(status_node);
	char *status_xml_c = XMLString::transcode(status_xml);
	
	send(s,status_xml_c,strlen(status_xml_c),0);
	
	XMLString::release(&status_xml);
	XMLString::release(&status_xml_c);
	serializer->release();
	xmldoc->release();
}

void WorkflowScheduler::init(void)
{
	DB db;
	
	db.Query("SELECT workflow_schedule_id FROM t_workflow_schedule WHERE workflow_schedule_active=1 ORDER BY workflow_schedule_id");
	
	num_wfs = db.NumRows();
	if(num_wfs==0)
	{
		wfs_ids = 0;
		wfs_wi_ids = 0;
		wfs_executing_instances = 0;
		return;
	}
	
	wfs_ids = new unsigned int[num_wfs];
	int i=0;
	while(db.FetchRow())
		wfs_ids[i++] = db.GetFieldInt(0);
	
	wfs_wi_ids = new unsigned int[num_wfs];
	memset(wfs_wi_ids,0,sizeof(unsigned int)*num_wfs);
	
	wfs_executing_instances = new WorkflowSchedule*[num_wfs];
	memset(wfs_executing_instances,0,sizeof(WorkflowScheduler*)*num_wfs);
}

int WorkflowScheduler::lookup(unsigned int *ids, int num_ids, unsigned int id)
{
	int begin,middle,end;

	begin=0;
	end=num_ids-1;
	while(begin<=end)
	{
		middle=(end+begin)/2;
		if(ids[middle]==id)
		{
			return middle;
		}
		if(ids[middle]<id)
			begin=middle+1;
		else
			end=middle-1;
	}

	return -1;
}

int WorkflowScheduler::lookup_wfs(unsigned int id)
{
	return lookup(wfs_ids,num_wfs,id);
}
