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

#include <ELogs/GC.h>
#include <DB/DB.h>
#include <DB/GarbageCollector.h>
#include <Logger/Logger.h>
#include <API/QueryHandlers.h>
#include <Utils/Date.h>
#include <Configuration/Configuration.h>

namespace ELogs
{

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	return (APIAutoInit *)new GC();
});

using namespace std;

void GC::APIReady()
{
	GarbageCollector::GetInstance()->RegisterPurgeHandler(GC::purge);
}

int GC::purge(time_t now)
{
	DB db("elog");
	DB db2(&db);
	
	int deleted_rows = 0;
	
	// Read configuration
	Configuration *config = Configuration::GetInstance();
	
	int limit = config->GetInt("gc.limit");
	int elogs_logs_retention = config->GetInt("gc.elogs.logs.retention");
	int elogs_triggers_retention = config->GetInt("gc.elogs.triggers.retention");
	string dbname = config->Get("elog.mysql.database");
	
	string date = Utils::Date::PastDate(elogs_triggers_retention * 86400, now);
	db.QueryPrintf("DELETE FROM t_alert_trigger WHERE alert_trigger_date <= %s LIMIT %i", {&date,&limit});
	deleted_rows += db.AffectedRows();
	
	db.QueryPrintf(
		"SELECT PARTITION_NAME FROM information_schema.partitions WHERE TABLE_SCHEMA=%s AND TABLE_NAME = 't_log' AND PARTITION_NAME IS NOT NULL ORDER BY PARTITION_DESCRIPTION DESC LIMIT 30 OFFSET %i",
		{&dbname, &elogs_logs_retention}
	);
	
	while(db.FetchRow())
	{
		db2.Query("ALTER TABLE t_elog DROP PARTITION "+db.GetField(0));
		db2.Query("ALTER TABLE t_value_char DROP PARTITION "+db.GetField(0));
		db2.Query("ALTER TABLE t_value_text DROP PARTITION "+db.GetField(0));
		db2.Query("ALTER TABLE t_value_itext DROP PARTITION "+db.GetField(0));
		db2.Query("ALTER TABLE t_value_ip DROP PARTITION "+db.GetField(0));
		db2.Query("ALTER TABLE t_value_pack DROP PARTITION "+db.GetField(0));
		db2.Query("ALTER TABLE t_int DROP PARTITION "+db.GetField(0));
		deleted_rows++;
	}
	
	return deleted_rows;
}


}
