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
#include <API/QueryHandlers.h>
#include <Utils/Date.h>

#include <string.h>
#include <unistd.h>
#include <time.h>

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	return (APIAutoInit *)new GarbageCollector();
});

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
	elogs_logs_retention = config->GetInt("gc.elogs.logs.retention");
	elogs_triggers_retention = config->GetInt("gc.elogs.triggers.retention");
	dbname = config->Get("mysql.database");
}

GarbageCollector::~GarbageCollector()
{
	Shutdown();
	WaitForShutdown();
}

void GarbageCollector::APIReady()
{
	if(enable)
	{
		// Create our thread
		gc_thread_handle = thread(GarbageCollector::gc_thread,this);
	}
}

void GarbageCollector::WaitForShutdown(void)
{
	if(!enable || gc_thread_handle.get_id()==thread::id())
		return; // gc_thread not launched, nothing to wait on
	
	gc_thread_handle.join();
}

void *GarbageCollector::gc_thread(GarbageCollector *gc)
{
	DB::StartThread();
	
	Logger::Log(LOG_NOTICE,"Garbage Collector started");
	
	while(true)
	{
		if(!gc->wait(gc->interval))
		{
			Logger::Log(LOG_NOTICE,"Shutdown in progress exiting Garbage Collector");
			
			DB::StopThread();
			
			return 0;
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
				
				if(!gc->wait(gc->delay))
				{
					Logger::Log(LOG_NOTICE,"Shutdown in progress exiting Garbage Collector");
					
					DB::StopThread();
					
					return 0;
				}
			}
		} while(deleted_rows>0);
	}
}

int GarbageCollector::purge(time_t now)
{
	// Purge
	try
	{
		DB db;
		DB db2(&db);
		string date;
		int deleted_rows = 0;
		
		// Purge workflows
		date = Utils::Date::PastDate(workflowinstance_retention * 86400, now);
		db.QueryPrintf("DELETE FROM t_workflow_instance WHERE workflow_instance_status='TERMINATED' AND workflow_instance_end <= %s LIMIT %i",&date,&limit);
		deleted_rows += db.AffectedRows();
		
		// Purge associated parameters
		db.QueryPrintf("DELETE wip FROM t_workflow_instance_parameters wip LEFT JOIN t_workflow_instance wi ON wip.workflow_instance_id=wi.workflow_instance_id WHERE wi.workflow_instance_id IS NULL");
		
		// Purge associated custom filters
		db.QueryPrintf("DELETE wif FROM t_workflow_instance_filters wif LEFT JOIN t_workflow_instance wi ON wif.workflow_instance_id=wi.workflow_instance_id WHERE wi.workflow_instance_id IS NULL");
		
		// Purge associated datastore entries
		db.QueryPrintf("DELETE data FROM t_datastore data LEFT JOIN t_workflow_instance wi ON data.workflow_instance_id=wi.workflow_instance_id WHERE wi.workflow_instance_id IS NULL");
		
		// Purge associated tags
		db.QueryPrintf("DELETE wit FROM t_workflow_instance_tag wit LEFT JOIN t_workflow_instance wi ON wit.workflow_instance_id=wi.workflow_instance_id WHERE wi.workflow_instance_id IS NULL");
		
		date = Utils::Date::PastDate(logs_retention * 86400, now);
		db.QueryPrintf("DELETE FROM t_log WHERE log_timestamp <= %s LIMIT %i",&date,&limit);
		deleted_rows += db.AffectedRows();
		
		date = Utils::Date::PastDate(logsapi_retention * 86400, now);
		db.QueryPrintf("DELETE FROM t_log_api WHERE log_api_timestamp <= %s LIMIT %i",&date,&limit);
		deleted_rows += db.AffectedRows();
		
		date = Utils::Date::PastDate(logsnotifications_retention * 86400, now);
		db.QueryPrintf("DELETE FROM t_log_notifications WHERE log_notifications_timestamp <= %s LIMIT %i",&date,&limit);
		deleted_rows += db.AffectedRows();
		
		date = Utils::Date::PastDate(uniqueaction_retention * 86400, now);
		db.QueryPrintf("DELETE FROM t_uniqueaction WHERE uniqueaction_time <= %s LIMIT %i",&date,&limit);
		deleted_rows += db.AffectedRows();
		
		date = Utils::Date::PastDate(elogs_triggers_retention * 86400, now);
		db.QueryPrintf("DELETE FROM t_alert_trigger WHERE alert_trigger_date <= %s LIMIT %i",&date,&limit);
		deleted_rows += db.AffectedRows();
		
		db.QueryPrintf("SELECT PARTITION_NAME FROM information_schema.partitions WHERE TABLE_SCHEMA=%s AND TABLE_NAME = 't_log' AND PARTITION_NAME IS NOT NULL ORDER BY PARTITION_DESCRIPTION DESC LIMIT 30 OFFSET %i", &dbname, &elogs_logs_retention);
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
