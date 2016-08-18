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
#include <Configuration.h>
#include <Statistics.h>
#include <Sockets.h>
#include <QueryResponse.h>
#include <QueryHandlers.h>
#include <AuthHandler.h>
#include <DB.h>
#include <ActiveConnections.h>
#include <tools.h>

#include <xercesc/sax2/SAX2XMLReader.hpp>
#include <xercesc/sax2/XMLReaderFactory.hpp>
#include <xercesc/sax2/DefaultHandler.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>
#include <xercesc/framework/Wrapper4InputSource.hpp>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>

#include <string>

using namespace std;

void *handle_connection(void *sp)
{
	int s  = *((int *)sp);
	delete (int *)sp;
	
	// Configure socket
	Configuration *config = Configuration::GetInstance();
	struct timeval tv;
	
	tv.tv_sec = config->GetInt("network.rcv.timeout");
	tv.tv_usec = 0;
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv,sizeof(struct timeval));
	
	tv.tv_sec = config->GetInt("network.snd.timeout");
	tv.tv_usec = 0;
	setsockopt(s, SOL_SOCKET, SO_SNDTIMEO,(struct timeval *)&tv,sizeof(struct timeval));
	
	socklen_t remote_addr_len;
	struct sockaddr_in remote_addr;
	char remote_addr_str[16] = "local";
	int remote_port = -1;
	remote_addr_len = sizeof(remote_addr);
	getpeername(s,(struct sockaddr*)&remote_addr,&remote_addr_len);
	if(remote_addr.sin_family==AF_INET)
	{
		inet_ntop(AF_INET,&(remote_addr.sin_addr),remote_addr_str,16);
		remote_port = ntohs(remote_addr.sin_port);
	}
	
	Logger::Log(LOG_INFO,"Accepted connection from %s:%d",remote_addr_str,remote_port);
	
	char buf[4096];
	int read_size;
	
	Statistics *stats = Statistics::GetInstance();
	stats->IncAcceptedConnections();
	
	// Init mysql library
	mysql_thread_init();
	
	try
	{
		AuthHandler auth_handler(s,remote_addr_str,remote_port);
		auth_handler.HandleAuth();
		
		send(s,"<ready />\n",10,0);
		
		while(true)
		{
			{
				QueryResponse response(s);
				
				SocketQuerySAX2Handler saxh("API");
				SocketSAX2Handler socket_sax2_handler(s);
				
				socket_sax2_handler.HandleQuery(&saxh);
				
				if(saxh.GetQueryGroup()=="quit")
					break;
				
				if(!QueryHandlers::GetInstance()->HandleQuery(saxh.GetQueryGroup(),&saxh, &response))
					throw Exception("API","Unknown command or action");
				
				response.SendResponse();
			}
		}
	}
	catch (Exception &e)
	{
		Logger::Log(LOG_WARNING,"Unexpected exception in context %s : %s\n",e.context.c_str(),e.error.c_str());
		
		QueryResponse response(s);
		response.SetError(e.error);
		response.SendResponse();
		
		Sockets::GetInstance()->UnregisterSocket(s);
		
		mysql_thread_end();
		
		// Notify that we exit
		ActiveConnections::GetInstance()->EndConnection(pthread_self());
		
		return 0;
		
	}
	
	Sockets::GetInstance()->UnregisterSocket(s);
	mysql_thread_end();
	
	// Notify that we exit
	ActiveConnections::GetInstance()->EndConnection(pthread_self());
	
	return 0;
}
