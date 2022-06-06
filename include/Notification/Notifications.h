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

#ifndef _NOTIFICATIONS_H_
#define _NOTIFICATIONS_H_

#include <API/APIObjectList.h>
#include <API/APIAutoInit.h>

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <vector>

class Notification;
class WorkflowInstance;
class XMLQuery;
class QueryResponse;
class User;

class Notifications:public APIObjectList<Notification>, public APIAutoInit
{
	private:
		struct st_notification_instance
		{
			unsigned int uid;
			std::string notif_instance_name;
			unsigned int notification_id;
			std::string notification_type;
		};
		
		static Notifications *instance;
		
		int max_concurrency;
		
		unsigned int uid;
		
		std::map<pid_t,st_notification_instance> notification_instances;
		
		std::string logs_directory;
		int logs_maxsize;
		bool logs_delete;
	
	public:
		
		Notifications();
		~Notifications();
		
		static Notifications *GetInstance() { return instance; }
		
		void Reload(bool notify = true);
		
		void Call(unsigned int notification_id, const std::string &notif_instance_name, const std::vector<std::string> &params, const nlohmann::json &j_data);
		void Exit(pid_t pid, int status, char retcode);
		
		static bool HandleQuery(const User &user, XMLQuery *query, QueryResponse *response);
		static void HandleReload(bool notify);
	
	private:
		void store_log(pid_t pid, unsigned int uid, unsigned int notification_id);
};

#endif
