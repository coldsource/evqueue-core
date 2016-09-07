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

#ifndef _USER_H_
#define _USER_H_

#include <string>
#include <map>
#include <vector>

class DB;
class SocketQuerySAX2Handler;
class QueryResponse;

class User
{
	struct user_right
	{
		bool edit;
		bool read;
		bool exec;
		bool kill;
	};
	
	std::string user_name;
	std::string user_password;
	std::string user_profile;
	std::string user_preferences;
	
	std::map<unsigned int,user_right> rights;
	
	public:
		static User anonymous;
		
		User();
		User(DB *db,const std::string &user_name);
		
		static void InitAnonymous();
		
		const std::string &GetName() const { return user_name; }
		const std::string &GetPassword() const { return user_password; }
		const std::string &GetProfile() const { return user_profile; }
		const std::string &GetPreferences() const { return user_preferences; }
		
		bool IsAdmin() const { return user_profile=="ADMIN"; }
		bool HasAccessToWorkflow(unsigned int workflow_id, const std::string &access_type) const;
		std::vector<int> GetReadAccessWorkflows() const;
		static bool InsufficientRights();
		
		static bool CheckUserName(const std::string &user_name);
		static void Get(const std::string &name, QueryResponse *response);
		static unsigned int Create(const std::string &name, const std::string &password, const std::string &profile);
		static void Edit(const std::string &name, const std::string &password, const std::string &profile);
		static void Delete(const std::string &name);
		static void ChangePassword(const std::string &name, const std::string &password);
		static void UpdatePreferences(const std::string &name, const std::string &preferences);
		
		static void ClearRights(const std::string &name);
		static void ListRights(const std::string &name, QueryResponse *response);
		static void GrantRight(const std::string &name, unsigned int workflow_id, bool edit, bool read, bool exec, bool kill);
		static void RevokeRight(const std::string &name, unsigned int workflow_id);
		
		static bool HandleQuery(const User &user, SocketQuerySAX2Handler *saxh, QueryResponse *response);
	
	private:
		static void create_edit_check(const std::string &name, const std::string &password, const std::string &profile);
};

#endif