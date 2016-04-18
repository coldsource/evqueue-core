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
			config->Get("mysql.database").c_str(),
			it->first.c_str()
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
