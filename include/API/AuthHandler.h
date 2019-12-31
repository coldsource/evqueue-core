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

#ifndef _AUTHHANDLER_H_
#define _AUTHHANDLER_H_

#include <string>

class User;
class XMLQuery;

class AuthHandler
{
	std::string remote_host;
	int remote_port;
	
	std::string challenge;
	
	public:
		AuthHandler();
		
		void SetRemote(const std::string &remote_host, int remote_port);
		
		std::string GetChallenge();
		User HandleChallenge(XMLQuery *query) const;
	
	private:
		std::string generate_challenge() const;
		int time_constant_strcmp(const std::string &str1, const std::string &str2) const;
};

#endif
