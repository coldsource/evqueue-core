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

#include <tools_db.h>
#include <tables.h>
#include <DB.h>
#include <Configuration.h>
#include <Exception.h>
#include <Logger.h>

using namespace std;

void tools_init_db(void)
{
	DB db;
	
	Configuration *config = Configuration::GetInstance();
	
	map<string,string>::iterator it;
	for(it=evqueue_tables.begin();it!=evqueue_tables.end();++it)
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
				db.Query("INSERT INTO t_queue(queue_name,queue_concurrency) VALUES('default',1);");
			else if(it->first=="t_user")
				db.Query("INSERT INTO t_user VALUES('admin',SHA1('admin'),'ADMIN');");
		}
		else
			if(string(db.GetField(0))!="v" EVQUEUE_VERSION)
				throw Exception("DB Init","Wrong table version, should be " EVQUEUE_VERSION);
	}
}
