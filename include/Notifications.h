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

#include <map>
#include <string>
#include <pthread.h>

class Notification;
class WorkflowInstance;

class Notifications
{
	private:
		struct st_notification_instance
		{
			unsigned int workflow_instance_id;
			std::string notification_type;
		};
		
		static Notifications *instance;
		
		pthread_mutex_t lock;
		
		int max_concurrency;
		
		std::map<unsigned int,Notification *> notifications;
		std::map<pid_t,st_notification_instance> notification_instances;
	
	public:
		
		Notifications();
		~Notifications();
		
		static Notifications *GetInstance() { return instance; }
		
		void Reload(void);
		Notification GetNotification(unsigned int id);
		
		void Call(unsigned int notification_id, WorkflowInstance *workflow_instance);
		void Exit(pid_t pid, int status, char retcode);
};

#endif
