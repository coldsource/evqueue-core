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
#include <UniqueAction.h>

#include <string.h>
#include <unistd.h>
#include <time.h>
#include <mysql/mysql.h>

using namespace std;

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
	logsapi_retention = Configuration::GetInstance()->GetInt("gc.logsapi.retention");
	logsnotifications_retention = Configuration::GetInstance()->GetInt("gc.logsnotifications.retention");
	uniqueaction_retention = Configuration::GetInstance()->GetInt("gc.uniqueaction.retention");
	
	is_shutting_down = false;
	
	if(enable)
	{
		// Create our thread
		gc_thread_handle = thread(GarbageCollector::gc_thread,this);
	}
}

void GarbageCollector::Shutdown(void)
{
	unique_lock<mutex> llock(lock);
	
	is_shutting_down = true;
}

void GarbageCollector::WaitForShutdown(void)
{
	if(!enable)
		return; // gc_thread not launched, nothing to wait on
	
	gc_thread_handle.join();
}

void *GarbageCollector::gc_thread(GarbageCollector *gc)
{
	mysql_thread_init();
	
	Logger::Log(LOG_INFO,"Garbage Collector started");
	
	while(true)
	{
		for(int iinterval=0;iinterval<gc->interval;iinterval++)
		{
			// Wait for GC interval seconds by steps of 1 second to allow shutdown detection
			unique_lock<mutex> llock(gc->lock);
			
			if(gc->is_shutting_down)
			{
				Logger::Log(LOG_INFO,"Shutdown in progress exiting Garbage Collector");
				
				mysql_thread_end();
				
				return 0;
			}
			
			llock.unlock();
			
			sleep(1);
		}
		
		UniqueAction uaction("gc",gc->interval);
		if(!uaction.IsElected())
			continue;
		
		int deleted_rows;
		
		time_t now;
		time(&now);
		
		do
		{
			deleted_rows = gc->purge(now);
			if(deleted_rows)
			{
				Logger::Log(LOG_NOTICE,"GarbageCollector: Removed %d expired entries",deleted_rows);
				
				for(int idelay=0;idelay<gc->delay;idelay++)
				{
					unique_lock<mutex> llock(gc->lock);
		
					if(gc->is_shutting_down)
					{
						Logger::Log(LOG_INFO,"Shutdown in progress exiting Garbage Collector");
						return 0;
					}
					
					llock.unlock();
					
					sleep(1);
				}
			}
		} while(deleted_rows>0);
	}
}

int GarbageCollector::purge(time_t now)
{
	// Compute dates
	struct tm wfi_t;
	struct tm logs_t;
	struct tm logsapi_t;
	struct tm logsnotifications_t;
	struct tm uniqueaction_t;
	time_t wfi,logs,logsapi,logsnotifications,uniqueaction;
	
	wfi = now - workflowinstance_retention*86400;
	localtime_r(&wfi,&wfi_t);
	
	logs = now - logs_retention*86400;
	localtime_r(&logs,&logs_t);
	
	logsapi = now - logsapi_retention*86400;
	localtime_r(&logsapi,&logsapi_t);
	
	logsnotifications = now - logsnotifications_retention*86400;
	localtime_r(&logsnotifications,&logsnotifications_t);
	
	uniqueaction = now - uniqueaction_retention*86400;
	localtime_r(&uniqueaction,&uniqueaction_t);
	
	// Purge
	try
	{
		DB db;
		char buf[32];
		int deleted_rows = 0;
		
		// Purge workflows
		strftime(buf,32,"%Y-%m-%d %H:%M:%S",&wfi_t);
		db.QueryPrintfC("DELETE FROM t_workflow_instance WHERE workflow_instance_status='TERMINATED' AND workflow_instance_end <= %s LIMIT %i",buf,&limit);
		deleted_rows += db.AffectedRows();
		
		// Purge associated parameters
		db.QueryPrintfC("DELETE wip FROM t_workflow_instance_parameters wip LEFT JOIN t_workflow_instance wi ON wip.workflow_instance_id=wi.workflow_instance_id WHERE wi.workflow_instance_id IS NULL");
		
		// Purge associated datastore entries
		db.QueryPrintfC("DELETE data FROM t_datastore data LEFT JOIN t_workflow_instance wi ON data.workflow_instance_id=wi.workflow_instance_id WHERE wi.workflow_instance_id IS NULL");
		
		// Purge associated tags
		db.QueryPrintfC("DELETE wit FROM t_workflow_instance_tag wit LEFT JOIN t_workflow_instance wi ON wit.workflow_instance_id=wi.workflow_instance_id WHERE wi.workflow_instance_id IS NULL");
		
		strftime(buf,32,"%Y-%m-%d %H:%M:%S",&logs_t);
		db.QueryPrintfC("DELETE FROM t_log WHERE log_timestamp <= %s LIMIT %i",buf,&limit);
		deleted_rows += db.AffectedRows();
		
		strftime(buf,32,"%Y-%m-%d %H:%M:%S",&logsapi_t);
		db.QueryPrintfC("DELETE FROM t_log_api WHERE log_api_timestamp <= %s LIMIT %i",buf,&limit);
		deleted_rows += db.AffectedRows();
		
		strftime(buf,32,"%Y-%m-%d %H:%M:%S",&logsnotifications_t);
		db.QueryPrintfC("DELETE FROM t_log_notifications WHERE log_notifications_timestamp <= %s LIMIT %i",buf,&limit);
		deleted_rows += db.AffectedRows();
		
		strftime(buf,32,"%Y-%m-%d %H:%M:%S",&uniqueaction_t);
		db.QueryPrintfC("DELETE FROM t_uniqueaction WHERE uniqueaction_time <= %s LIMIT %i",buf,&limit);
		deleted_rows += db.AffectedRows();
		
		return deleted_rows;
	}
	catch(Exception &e)
	{
		Logger::Log(LOG_ERR,"[ GarbageCollector ] Error trying to clean old entries. DB returned : "+e.error);
		// Error connecting to database. Will try again later
		return 0;
	}
}
