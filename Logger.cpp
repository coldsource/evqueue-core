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

#include <Logger.h>
#include <Configuration.h>
#include <DB.h>

#include <string.h>
#include <syslog.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

Logger *Logger::instance = 0;

Logger::Logger()
{
	Configuration *config = Configuration::GetInstance();
	
	log_syslog = Configuration::GetInstance()->GetBool("logger.syslog.enable");
	
	log_db = Configuration::GetInstance()->GetBool("logger.db.enable");
	
	instance = this;
}

void Logger::Log(int level,const char *msg,...)
{
	char buf[1024];
	va_list ap;
	int n;
	
	va_start(ap, msg);
	n = vsnprintf(buf,1024,msg,ap);
	va_end(ap);
	
	if(n<0)
		return;
	
	if(instance->log_syslog)
		syslog(LOG_NOTICE,buf);
	
	if(instance->log_db)
	{
		DB db;
		db.QueryPrintf("INSERT INTO t_log(log_level,log_message,log_timestamp) VALUES(%i,%s,NOW())",&level,buf);
	}
}
