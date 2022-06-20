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

#include <API/handle_connection.h>
#include <API/XMLQuery.h>
#include <Exception/Exception.h>
#include <IO/NetworkInputSource.h>
#include <Logger/Logger.h>
#include <User/User.h>
#include <Configuration/ConfigurationEvQueue.h>
#include <API/Statistics.h>
#include <API/QueryResponse.h>
#include <API/QueryHandlers.h>
#include <API/AuthHandler.h>
#include <DB/DB.h>
#include <API/ActiveConnections.h>
#include <API/tools.h>
#include <API/APISession.h>
#include <API/QueryHandlers.h>
#include <IO/NetworkConnections.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>

#include <string>

#include <XML/XMLString.h>

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	ConfigurationEvQueue *config = ConfigurationEvQueue::GetInstance();
	
	// Create TCP and UNIX sockets
	NetworkConnections::t_stream_handler api_handler = [](int s) {
		if(ActiveConnections::GetInstance()->GetAPINumber()>=ConfigurationEvQueue::GetInstance()->GetInt("network.connections.max"))
		{
			close(s);
			
			Logger::Log(LOG_WARNING,"Max API connections reached, dropping connection");
		}
		
		ActiveConnections::GetInstance()->StartAPIConnection(s);
	};
	
	NetworkConnections *nc = NetworkConnections::GetInstance();
	if(config->Get("network.bind.ip")!="")
		nc->RegisterTCP("API (tcp)", config->Get("network.bind.ip"), config->GetInt("network.bind.port"), config->GetInt("network.listen.backlog"), api_handler);
	
	if(config->Get("network.bind.path")!="")
		nc->RegisterUNIX("API (unix)", config->Get("network.bind.path"), config->GetInt("network.listen.backlog"), api_handler);
	
	return (APIAutoInit *)0;
});

using namespace std;

void handle_connection(int s)
{
	// Configure socket
	Configuration *config = ConfigurationEvQueue::GetInstance();
	struct timeval tv;
	
	tv.tv_sec = config->GetInt("network.rcv.timeout");
	tv.tv_usec = 0;
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv,sizeof(struct timeval));
	
	tv.tv_sec = config->GetInt("network.snd.timeout");
	tv.tv_usec = 0;
	setsockopt(s, SOL_SOCKET, SO_SNDTIMEO,(struct timeval *)&tv,sizeof(struct timeval));
	
	char buf[4096];
	int read_size;
	
	Statistics *stats = Statistics::GetInstance();
	stats->IncAPIAcceptedConnections();
	
	// Init mysql library
	DB::StartThread();
	
	try
	{
		APISession session("API",s);
		
		session.SendChallenge();
		if(session.GetStatus()==APISession::en_status::WAITING_CHALLENGE_RESPONSE)
		{
			XMLQuery query("Authentication Handler",s);
			session.ChallengeReceived(&query);
		}
		
		session.SendGreeting();
		
		while(true)
		{
			{
				// Wait for a query
				Logger::Log(LOG_DEBUG,"API : Waiting request");
				XMLQuery query("API",s);
				
				if(session.QueryReceived(&query))
					break; // Quit requested
				
				session.SendResponse();
			}
		}
	}
	catch (Exception &e)
	{
		stats->IncAPIExceptions();
		Logger::Log(LOG_INFO,"Unexpected exception in context "+e.context+" : "+e.error);
		
		QueryResponse response(s);
		response.SetError(e.error);
		if(e.code!="")
			response.SetErrorCode(e.code);
		else
			response.SetErrorCode("UNEXPECTED_EXCEPTION");
		response.SendResponse();
	}
	
	DB::StopThread();
	
	// Notify that we exit
	ActiveConnections::GetInstance()->EndAPIConnection(this_thread::get_id());
	close(s);
	
	return;
}
