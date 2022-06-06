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

#ifndef _LOGGERAPI_H_
#define _LOGGERAPI_H_

#include <API/APIAutoInit.h>

#include <string>

class User;

class LoggerAPI: public APIAutoInit
{
	static LoggerAPI *instance;
	
	bool enabled;
	const std::string &node_name;
	
public:
	LoggerAPI();
	
	static LoggerAPI *GetInstance() { return instance; }
	
	static void LogAction(const User &user, unsigned int object_id, const std::string &object_type, const std::string &group, const std::string &action);
};

#endif
