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
class SocketQuerySAX2Handler;
class QueryResponse;

class Notification
{
	unsigned int id;
	unsigned int type_id;
	std::string notification_monitor_path;
	std::string notification_binary;
	std::string notification_name;
	std::string notification_configuration;
	std::string unix_socket_path;
	
	public:

		Notification(DB *db,unsigned int notification_id);
		
		unsigned int &GetID() { return id; }
		unsigned int &GetTypeID() { return type_id; }
		const std::string &GetName() { return notification_name; }
		const std::string &GetBinary() { return notification_binary; }
		const std::string &GetConfiguration() { return notification_configuration; }
		
		pid_t Call(WorkflowInstance *workflow_instance);
		
		static void Get(unsigned int id,QueryResponse *response);
		static void Create(unsigned int type_id,const std::string &name, const std::string parameters);
		static void Edit(unsigned int id,unsigned int type_id,const std::string &name, const std::string parameters);
		static void Delete(unsigned int id);
		
		static bool HandleQuery(SocketQuerySAX2Handler *saxh, QueryResponse *response);
	
	private:
		void free(void);
		
		static void create_edit_check(unsigned int type_id,const std::string &name, const std::string parameters);
};

#endif