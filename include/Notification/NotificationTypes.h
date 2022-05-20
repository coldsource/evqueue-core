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

#ifndef _NOTIFICATIONTYPES_H_
#define _NOTIFICATIONTYPES_H_

#include <API/APIObjectList.h>
#include <API/APIAutoInit.h>

#include <map>
#include <string>

class NotificationType;
class XMLQuery;
class QueryResponse;
class User;

class NotificationTypes:public APIObjectList<NotificationType>, public APIAutoInit
{
	public:
		typedef void (*t_delete_handler)(unsigned int notification_type_id);

	private:
		static NotificationTypes *instance;
		
		std::map<std::string, t_delete_handler> delete_handlers;
	
	public:
		
		NotificationTypes();
		~NotificationTypes();
		
		static NotificationTypes *GetInstance() { return instance; }
		
		void RegisterDeleteHandler(const std::string &scope, t_delete_handler delete_handler);
		void HandleDelete(unsigned int id);
		
		void Reload(bool notify = true);
		void SyncBinaries(bool notify = true);

		static bool HandleQuery(const User &user, XMLQuery *query, QueryResponse *response);
};

#endif
