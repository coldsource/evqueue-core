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

#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <syslog.h>

#include <string>

class Logger
{
	private:
		static Logger *instance;
		
		const std::string &node_name;
		
		bool log_syslog;
		int syslog_filter;
		bool log_db;
		int db_filter;
	
	public:
		Logger();
		static Logger *GetInstance() { return instance; }
		
		static void Log(int level,const char *msg,...);
		static int GetIntegerLogLevel(const std::string &log_level) { return parse_log_level(log_level); }
		
	private:
		static int parse_log_level(const std::string &log_level);
};

#endif
