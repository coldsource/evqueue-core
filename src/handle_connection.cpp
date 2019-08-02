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

#include <handle_connection.h>
#include <SocketQuerySAX2Handler.h>
#include <Exception.h>
#include <NetworkInputSource.h>
#include <Logger.h>
#include <User.h>
#include <ConfigurationEvQueue.h>
#include <Statistics.h>
#include <Sockets.h>
#include <QueryResponse.h>
#include <QueryHandlers.h>
#include <APISession.h>
#include <DB.h>
#include <ActiveConnections.h>
#include <tools.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>

#include <string>

#include <XMLString.h>
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
	stats->IncAcceptedConnections();
	
	// Init mysql library
	mysql_thread_init();
	
	try
	{
		APISession session("API",s);
		
		session.SendChallenge();
		if(session.GetStatus()==APISession::en_status::WAITING_CHALLENGE_RESPONSE)
		{
			SocketQuerySAX2Handler saxh("Authentication Handler");
			SocketSAX2Handler socket_sax2_handler(s);
			socket_sax2_handler.HandleQuery(&saxh);
			session.ChallengeReceived(&saxh);
		}
		
		session.SendGreeting();
		
		while(true)
		{
			{
				// Wait for a query
				SocketQuerySAX2Handler saxh("API");
				SocketSAX2Handler socket_sax2_handler(s);
				
				Logger::Log(LOG_DEBUG,"API : Waiting request");
				
				socket_sax2_handler.HandleQuery(&saxh);
				
				if(session.QueryReceived(&saxh))
					break; // Quit requested
				
				session.SendResponse();
			}
		}
	}
	catch (Exception &e)
	{
		Logger::Log(LOG_INFO,"Unexpected exception in context "+e.context+" : "+e.error);
		
		QueryResponse response(s);
		response.SetError(e.error);
		if(e.code!="")
			response.SetErrorCode(e.code);
		else
			response.SetErrorCode("UNEXPECTED_EXCEPTION");
		response.SendResponse();
		
		Sockets::GetInstance()->UnregisterSocket(s);
		
		mysql_thread_end();
		
		// Notify that we exit
		ActiveConnections::GetInstance()->EndConnection(this_thread::get_id());
		
		return;
		
	}
	
	Sockets::GetInstance()->UnregisterSocket(s);
	mysql_thread_end();
	
	// Notify that we exit
	ActiveConnections::GetInstance()->EndConnection(this_thread::get_id());
	
	return;
}
