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

#include <ELogs/ConfigurationELogs.h>
#include <DB/DBConfig.h>
#include <Exception/Exception.h>

#include <vector>

using namespace std;

namespace ELogs
{

static auto init = Configuration::GetInstance()->RegisterConfig(new ConfigurationELogs());
static auto initdb =  DBConfig::GetInstance()->RegisterConfigInit([](DBConfig *dbconf) {
	Configuration *config = Configuration::GetInstance();
	string host = config->Get("elog.mysql.host");
	string user = config->Get("elog.mysql.user");
	string password = config->Get("elog.mysql.password");
	string database = config->Get("elog.mysql.database");
	dbconf->RegisterConfig("elog", host, user, password, database);
});

ConfigurationELogs::ConfigurationELogs()
{
	entries["elog.mysql.database"] = "evqueue-elogs";
	entries["elog.mysql.host"] = "localhost";
	entries["elog.mysql.password"] = "";
	entries["elog.mysql.user"] = "";
	entries["elog.bind.ip"] = "*";
	entries["elog.bind.port"] = "5002";
	entries["elog.queue.size"] = "1000";
	entries["elog.bulk.size"] = "500";
	entries["elog.log.maxsize"] = "4K";
	
	entries["gc.elogs.logs.retention"] = "90";
	entries["gc.elogs.triggers.retention"] = "30";
}

ConfigurationELogs::~ConfigurationELogs()
{
}

void ConfigurationELogs::Check(void)
{
	check_int_entry("elog.bind.port");
	check_int_entry("elog.queue.size");
	check_int_entry("elog.bulk.size");
	
	check_int_entry("gc.elogs.logs.retention");
	check_int_entry("gc.elogs.triggers.retention");
	
	check_size_entry("elog.log.maxsize");
	
	if(Configuration::GetInstance()->Get("mysql.database")==Get("elog.mysql.database"))
		throw Exception("Configuration","mysql.database and elog.mysql.database cannot be the same");
	
	if(GetInt("gc.elogs.logs.retention")<2)
		throw Exception("Configuration","gc.elogs.logs.retention: cannot be less than 2");
	
	if(GetInt("gc.elogs.triggers.retention")<2)
		throw Exception("Configuration","gc.elogs.triggers.retention: cannot be less than 2");
}

}
