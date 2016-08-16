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

#include <GarbageCollector.h>
#include <Configuration.h>
#include <DB.h>
#include <Logger.h>
#include <Exception.h>

#include <string.h>
#include <unistd.h>
#include <time.h>
#include <mysql/mysql.h>

GarbageCollector::GarbageCollector()
{
	// Read configuration
	Configuration *config = Configuration::GetInstance();
	
	enable = Configuration::GetInstance()->GetBool("gc.enable");
	delay = Configuration::GetInstance()->GetInt("gc.delay");
	interval = Configuration::GetInstance()->GetInt("gc.interval");
	limit = Configuration::GetInstance()->GetInt("gc.limit");
	workflowinstance_retention = Configuration::GetInstance()->GetInt("gc.workflowinstance.retention");
	logs_retention = Configuration::GetInstance()->GetInt("gc.logs.retention");
	
	is_shutting_down = false;
	
	pthread_mutex_init(&lock,NULL);
	
	if(enable)
	{
		// Create our thread
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
		pthread_create(&gc_thread_handle, &attr, &GarbageCollector::gc_thread,this);
		
		pthread_setname_np(gc_thread_handle,"garbage_collect");
	}
}

void GarbageCollector::Shutdown(void)
{
	pthread_mutex_lock(&lock);
	
	is_shutting_down = true;
	
	pthread_mutex_unlock(&lock);
}

void GarbageCollector::WaitForShutdown(void)
{
	if(!enable)
		return; // gc_thread not launched, nothing to wait on
	
	pthread_join(gc_thread_handle,0);
}

void *GarbageCollector::gc_thread( void *context )
{
	GarbageCollector *gc = (GarbageCollector *)context;
	
	mysql_thread_init();
	
	Logger::Log(LOG_INFO,"Garbage Collector started");
	
	while(true)
	{
		for(int iinterval=0;iinterval<gc->interval;iinterval++)
		{
			pthread_mutex_lock(&gc->lock);
			
			if(gc->is_shutting_down)
			{
				Logger::Log(LOG_INFO,"Shutdown in progress exiting Garbage Collector");
				
				mysql_thread_end();
				
				return 0;
			}
			
			pthread_mutex_unlock(&gc->lock);
			
			sleep(1);
		}
		
		int deleted_rows;
		do
		{
			deleted_rows = gc->purge();
			if(deleted_rows)
			{
				Logger::Log(LOG_NOTICE,"GarbageCollector: Removed %d expired entries",deleted_rows);
				
				for(int idelay=0;idelay<gc->delay;idelay++)
				{
					pthread_mutex_lock(&gc->lock);
		
					if(gc->is_shutting_down)
					{
						Logger::Log(LOG_INFO,"Shutdown in progress exiting Garbage Collector");
						return 0;
					}
					
					pthread_mutex_unlock(&gc->lock);
					
					sleep(1);
				}
			}
		} while(deleted_rows>0);
	}
}

int GarbageCollector::purge(void)
{
	// Compute dates
	struct tm wfi_t;
	struct tm logs_t;
	time_t now,wfi,logs;
	
	time(&now);
	
	wfi = now - workflowinstance_retention*86400;
	localtime_r(&wfi,&wfi_t);
	
	logs = now - logs_retention*86400;
	localtime_r(&logs,&logs_t);
	
	// Purge
	try
	{
		DB db;
		char buf[32];
		int deleted_rows = 0;
		
		strftime(buf,32,"%Y-%m-%d %H:%M:%S",&wfi_t);
		db.QueryPrintfC("DELETE FROM t_workflow_instance WHERE workflow_instance_status='TERMINATED' AND workflow_instance_end <= %s LIMIT %i",buf,&limit);
		deleted_rows += db.AffectedRows();
		
		db.QueryPrintfC("DELETE wip FROM t_workflow_instance_parameters wip LEFT JOIN t_workflow_instance wi ON wip.workflow_instance_id=wi.workflow_instance_id WHERE wi.workflow_instance_id IS NULL");
		
		strftime(buf,32,"%Y-%m-%d %H:%M:%S",&logs_t);
		db.QueryPrintfC("DELETE FROM t_log WHERE log_timestamp <= %s LIMIT %i",buf,&limit);
		deleted_rows += db.AffectedRows();
		
		return deleted_rows;
	}
	catch(Exception &e)
	{
		Logger::Log(LOG_ERR,"[ GarbageCollector ] Error trying to clean old entries. DB returned : %s",e.error.c_str());
		// Error connecting to database. Will try again later
		return 0;
	}
}
