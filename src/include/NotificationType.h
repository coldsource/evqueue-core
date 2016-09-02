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

#ifndef _NOTIFICATIONTYPE_H_
#define _NOTIFICATIONTYPE_H_

#include <string>

class DB;
class SocketQuerySAX2Handler;
class QueryResponse;
class User;

class NotificationType
{
	unsigned int id;
	std::string name;
	std::string description;
	
	public:

		NotificationType(DB *db,unsigned int notification_type_id);
		
		unsigned int GetID() { return id; }
		const std::string &GetName() { return name; }
		const std::string &GetDescription() { return description; }
		const std::string &GetBinary() { return name; }
		
		static void PutFile(const std::string &filename,const std::string &data,bool base64_encoded=true);
		static void GetFile(const std::string &filename,std::string &data);
		static void GetFileHash(const std::string &filename,std::string &hash);
		static void RemoveFile(const std::string &filename);
		
		static void PutConfFile(const std::string &filename,const std::string &data,bool base64_encoded=true);
		static void GetConfFile(const std::string &filename,std::string &data);
		static void GetConfFileHash(const std::string &filename,std::string &hash);
		static void RemoveConfFile(const std::string &filename);
		
		static void Register(const std::string &name, const std::string &description, const std::string binary_content);
		static void Unregister(unsigned int id);
		
		static void GetConf(unsigned int id, QueryResponse *response);
		static void SetConf(unsigned int id, const std::string &data);
		
		static bool HandleQuery(const User &user, SocketQuerySAX2Handler *saxh, QueryResponse *response);
};

#endif
