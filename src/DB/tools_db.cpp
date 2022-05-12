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

#include <DB/tools_db.h>
#include <DB/tables.h>
#include <DB/DB.h>
#include <Configuration/ConfigurationEvQueue.h>
#include <Exception/Exception.h>
#include <Logger/Logger.h>
#include <DOM/DOMDocument.h>
#include <DOM/DOMXPathResult.h>

#include <string>
#include <vector>
#include <memory>

using namespace std;

void tools_init_db(void)
{
	Configuration *config = ConfigurationEvQueue::GetInstance();
	
	// evQueue core
	{
		DB db;
		for(auto it=evqueue_tables.begin();it!=evqueue_tables.end();++it)
		{
			db.QueryPrintf(
				"SELECT table_comment FROM INFORMATION_SCHEMA.TABLES WHERE table_schema=%s AND table_name=%s",
				&config->Get("mysql.database"),
				&it->first
			);
			
			if(!db.FetchRow())
			{
				Logger::Log(LOG_NOTICE,"Table %s does not exists, creating it...",it->first.c_str());
				
				db.Query(it->second.c_str());
				
				if(it->first=="t_queue")
					db.Query("INSERT INTO t_queue(queue_name,queue_concurrency,queue_scheduler) VALUES('default',1,'default');");
				else if(it->first=="t_user")
					db.Query("INSERT INTO t_user(user_login,user_password,user_profile,user_preferences) VALUES('admin',SHA1('admin'),'ADMIN','');");
			}
			else
			{
				if(string(db.GetField(0))!="v" EVQUEUE_VERSION)
				{
					if(db.GetField(0)=="v3.0" && EVQUEUE_VERSION=="3.1")
						tools_upgrade_v30_v31();
					else if(db.GetField(0)=="v3.1" && EVQUEUE_VERSION=="3.2")
						tools_upgrade_v31_v32();
					else if(db.GetField(0)=="v3.2" && EVQUEUE_VERSION=="3.3")
						tools_upgrade_v32_v33();
					else
						throw Exception("DB Init","Wrong table version, should be " EVQUEUE_VERSION);
				}
				
				if(it->first=="t_notification")
					tools_upgrade_t_notification();
			}
		}
	}
	
	// evQueue elogs module
	{
		DB db("elog");
		for(auto it=evqueue_elogs_tables.begin();it!=evqueue_elogs_tables.end();++it)
		{
			db.QueryPrintf(
				"SELECT table_comment FROM INFORMATION_SCHEMA.TABLES WHERE table_schema=%s AND table_name=%s",
				&config->Get("elog.mysql.database"),
				&it->first
			);
			
			if(!db.FetchRow())
			{
				Logger::Log(LOG_NOTICE,"ELogs table %s does not exists, creating it...",it->first.c_str());
				
				db.Query(it->second.c_str());
			}
		}
	}
}

void tools_upgrade_v30_v31(void)
{
	Logger::Log(LOG_NOTICE,"Detected v3.0 tables, upgrading scheme to v3.1");
	
	DB db;
	
	// Update tables version
	for(auto it=evqueue_tables.begin();it!=evqueue_tables.end();++it)
	{
		string version = "v" EVQUEUE_VERSION;
		db.QueryPrintf("ALTER TABLE "+it->first+" COMMENT=%s",&version);
	}
}

void tools_upgrade_v31_v32(void)
{
	Logger::Log(LOG_NOTICE,"Detected v3.1 tables, upgrading scheme to v3.2");
	
	DB db;
	
	// Update tables version
	for(auto it=evqueue_tables.begin();it!=evqueue_tables.end();++it)
	{
		string version = "v" EVQUEUE_VERSION;
		db.QueryPrintf("ALTER TABLE "+it->first+" COMMENT=%s",&version);
	}
}

void tools_upgrade_v32_v33(void)
{
	Logger::Log(LOG_NOTICE,"Detected v3.2 tables, upgrading scheme to v3.3");
	
	DB db;
	
	db.Query("ALTER TABLE `t_log_api` CHANGE `log_api_object_type` `log_api_object_type` ENUM('Workflow','WorkflowSchedule','RetrySchedule','User','Tag','Queue', 'Channel', 'ChannelGroup', 'Alert') CHARACTER SET ascii COLLATE ascii_bin NOT NULL");
	
	db.Query("ALTER TABLE `t_notification_type` ADD `notification_type_scope` ENUM('WORKFLOW', 'ELOGS') CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT 'WORKFLOW' AFTER `notification_type_id`");
	
	// Update tables version
	for(auto it=evqueue_tables.begin();it!=evqueue_tables.end();++it)
	{
		string version = "v" EVQUEUE_VERSION;
		db.QueryPrintf("ALTER TABLE "+it->first+" COMMENT=%s",&version);
	}
}

void tools_upgrade_t_notification()
{
	DB db;
	db.QueryPrintf(
		"SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS WHERE table_schema=%s AND table_name='t_notification' AND column_name='notification_subscribe_all'",
		&ConfigurationEvQueue::GetInstance()->Get("mysql.database")
	);
	
	db.FetchRow();
	if(db.GetFieldInt(0)==0)
		db.Query("ALTER TABLE t_notification ADD COLUMN `notification_subscribe_all` int(10) unsigned NOT NULL DEFAULT 0 AFTER notification_name");
}
