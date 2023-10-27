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

#ifndef _APISESSION_H_
#define _APISESSION_H_

#include <User/User.h>
#include <API/AuthHandler.h>
#include <API/QueryResponse.h>

#include <libwebsockets.h>

#include <string>
#include <queue>
#include <mutex>

class XMLQuery;

class APISession
{
	public:
		enum en_status
		{
			INITIALIZED,
			WAITING_CHALLENGE_RESPONSE,
			AUTHENTICATED,
			READY,
		};
	
	private:
		std::mutex lock;
		
		bool deleted = false;
		int acquired = 0;
		
		en_status status;
		std::string context;
		
		std::string remote_host;
		int remote_port = 0;
		
		AuthHandler ah;
		User user;
		
		std::queue<QueryResponse> responses;
		
		int s;
		struct lws *wsi;
		
		void init(const std::string &context);
	
	public:
		APISession(const std::string &context, int s);
		APISession(const std::string &context, struct lws *wsi);
		
		void Acquire();
		bool Release();
		bool FlagDeleted();
		
		const User &GetUser() { return user; }
		en_status GetStatus() { return status; }
		
		void SendChallenge();
		void ChallengeReceived(XMLQuery *query);
		
		void SendGreeting();
		
		bool QueryReceived(XMLQuery *query, int external_id = 0, const std::string &object_id = "", unsigned long long event_id = 0);
		bool SendResponse();
		void Query(const std::string &xml, int external_id=0, const std::string &object_id="", unsigned long long event_id=0);
};

#endif
