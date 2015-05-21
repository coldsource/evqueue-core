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

#ifndef _NOTIFICATION_H_
#define _NOTIFICATION_H_

#include <unistd.h>

#include <string>

class DB;
class WorkflowInstance;

class Notification
{
	std::string notification_monitor_path;
	std::string notification_binary;
	std::string notification_name;
	std::string notification_configuration;
	
	public:

		Notification(DB *db,unsigned int notification_id);
		
		const char *GetBinary() { return notification_binary.c_str(); }
		const char *GetConfiguration() { return notification_configuration.c_str(); }
		
		void Call(WorkflowInstance *workflow_instance);
	
	private:
		void free(void);
};

#endif