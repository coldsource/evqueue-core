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

#include <ping.h>

#include <SocketQuerySAX2Handler.h>
#include <QueryResponse.h>
#include <Configuration.h>

#include <time.h>

using namespace std;

extern time_t evqueue_start_time;

bool ping_handle_query(const User &user, SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	const std::map<std::string,std::string> attrs = saxh->GetRootAttributes();
	
	response->SetAttribute("version",EVQUEUE_VERSION);
#ifdef USELIBGIT2
	response->SetAttribute("git-support","enabled");
#else
	response->SetAttribute("git-support","disabled");
#endif
	
	time_t now;
	time(&now);
	
	response->SetAttribute("uptime",to_string(now-evqueue_start_time));
	
	return true;
}
