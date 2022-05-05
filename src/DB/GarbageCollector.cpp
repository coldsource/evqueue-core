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

#include <DB/GarbageCollector.h>
#include <Configuration/ConfigurationEvQueue.h>
#include <DB/DB.h>
#include <Logger/Logger.h>
#include <Exception/Exception.h>
#include <Cluster/UniqueAction.h>

#include <string.h>
#include <unistd.h>
#include <time.h>

using namespace std;

GarbageCollector::GarbageCollector()
{
	// Read configuration
	Configuration *config = ConfigurationEvQueue::GetInstance();
	
	enable = config->GetBool("gc.enable");
	delay = config->GetInt("gc.delay");
	interval = config->GetInt("gc.interval");
	limit = config->GetInt("gc.limit");
	workflowinstance_retention = config->GetInt("gc.workflowinstance.retention");
	logs_retention = config->GetInt("gc.logs.retention");
	logsapi_retention = config->GetInt("gc.logsapi.retention");
	logsnotifications_retention = config->GetInt("gc.logsnotifications.retention");
	uniqueaction_retention = config->GetInt("gc.uniqueaction.retention");
	elogs_retention = config->GetInt("gc.elogs.retention");
	dbname = config->Get("mysql.database");
	
	is_shutting_down = false;
	
	if(enable)
	{
		// Create our thread
		gc_thread_handle = thread(GarbageCollector::gc_thread,this);
	}
}

GarbageCollector::~GarbageCollector()
{
	Shutdown();
	WaitForShutdown();
}

void GarbageCollector::Shutdown(void)
{
	unique_lock<mutex> llock(lock);
	
	is_shutting_down = true;
	
	shutdown_requested.notify_one();
}

void GarbageCollector::WaitForShutdown(void)
{
	if(!enable)
		return; // gc_thread not launched, nothing to wait on
	
	gc_thread_handle.join();
}

void *GarbageCollector::gc_thread(GarbageCollector *gc)
{
	DB::StartThread();
	
	Logger::Log(LOG_NOTICE,"Garbage Collector started");
	
	while(true)
	{
		unique_lock<mutex> llock(gc->lock);
		
		cv_status ret;
		if(!gc->is_shutting_down)
			ret = gc->shutdown_requested.wait_for(llock, chrono::seconds(gc->interval));
		
		llock.unlock();
		
		if(gc->is_shutting_down)
		{
			Logger::Log(LOG_NOTICE,"Shutdown in progress exiting Garbage Collector");
			
			DB::StopThread();
			
			return 0;
		}
		
		if(ret!=cv_status::timeout)
			continue; // Suprious interrupt, continue
		
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
				
				unique_lock<mutex> llock(gc->lock);
				
				if(!gc->is_shutting_down)
					gc->shutdown_requested.wait_for(llock, chrono::seconds(gc->delay));
				
				llock.unlock();
	
				if(gc->is_shutting_down)
				{
					Logger::Log(LOG_INFO,"Shutdown in progress exiting Garbage Collector");
					
					DB::StopThread();
					
					return 0;
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
		DB db2(&db);
		char buf[32];
		int deleted_rows = 0;
		
		// Purge workflows
		strftime(buf,32,"%Y-%m-%d %H:%M:%S",&wfi_t);
		db.QueryPrintfC("DELETE FROM t_workflow_instance WHERE workflow_instance_status='TERMINATED' AND workflow_instance_end <= %s LIMIT %i",buf,&limit);
		deleted_rows += db.AffectedRows();
		
		// Purge associated parameters
		db.QueryPrintfC("DELETE wip FROM t_workflow_instance_parameters wip LEFT JOIN t_workflow_instance wi ON wip.workflow_instance_id=wi.workflow_instance_id WHERE wi.workflow_instance_id IS NULL");
		
		// Purge associated custom filters
		db.QueryPrintfC("DELETE wif FROM t_workflow_instance_filters wif LEFT JOIN t_workflow_instance wi ON wif.workflow_instance_id=wi.workflow_instance_id WHERE wi.workflow_instance_id IS NULL");
		
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
		
		db.QueryPrintf("SELECT PARTITION_NAME FROM information_schema.partitions WHERE TABLE_SCHEMA=%s AND TABLE_NAME = 't_log' AND PARTITION_NAME IS NOT NULL ORDER BY PARTITION_DESCRIPTION DESC LIMIT 30 OFFSET %i", &dbname, &elogs_retention);
		while(db.FetchRow())
		{
			db2.QueryPrintf("ALTER TABLE t_elog DROP PARTITION "+db.GetField(0));
			db2.QueryPrintf("ALTER TABLE t_value_char DROP PARTITION "+db.GetField(0));
			db2.QueryPrintf("ALTER TABLE t_value_text DROP PARTITION "+db.GetField(0));
			db2.QueryPrintf("ALTER TABLE t_value_itext DROP PARTITION "+db.GetField(0));
			db2.QueryPrintf("ALTER TABLE t_value_ip DROP PARTITION "+db.GetField(0));
			db2.QueryPrintf("ALTER TABLE t_value_pack DROP PARTITION "+db.GetField(0));
			db2.QueryPrintf("ALTER TABLE t_int DROP PARTITION "+db.GetField(0));
			deleted_rows++;
		}
		
		return deleted_rows;
	}
	catch(Exception &e)
	{
		Logger::Log(LOG_ERR,"[ GarbageCollector ] Error trying to clean old entries. DB returned : "+e.error);
		// Error connecting to database. Will try again later
		return 0;
	}
}
