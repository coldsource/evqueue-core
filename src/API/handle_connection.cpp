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
#include <API/SocketQuerySAX2Handler.h>
#include <Exception/Exception.h>
#include <IO/NetworkInputSource.h>
#include <Logger/Logger.h>
#include <User/User.h>
#include <Configuration/ConfigurationEvQueue.h>
#include <API/Statistics.h>
#include <IO/Sockets.h>
#include <API/QueryResponse.h>
#include <API/QueryHandlers.h>
#include <API/AuthHandler.h>
#include <DB/DB.h>
#include <API/ActiveConnections.h>
#include <API/tools.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>

#include <string>

#include <XML/XMLString.h>
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
		User user = auth_handler.HandleAuth();
		
		QueryResponse ready_response(s,"ready");
		ready_response.SetAttribute("profile",user.GetProfile());
		ready_response.SetAttribute("version",EVQUEUE_VERSION);
		ready_response.SetAttribute("node",ConfigurationEvQueue::GetInstance()->Get("cluster.node.name"));
		ready_response.SendResponse();
		
		while(true)
		{
			{
				QueryResponse response(s);
				
				SocketQuerySAX2Handler saxh("API");
				SocketSAX2Handler socket_sax2_handler(s);
				
				Logger::Log(LOG_DEBUG,"API : Waiting request");
				
				
				socket_sax2_handler.HandleQuery(&saxh);
				
				if(saxh.GetQueryGroup()=="quit")
				{
					Logger::Log(LOG_DEBUG,"API : Received quit command, exiting channel");
					break;
				}
				
				try
				{
					if(!QueryHandlers::GetInstance()->HandleQuery(user, saxh.GetQueryGroup(),&saxh, &response))
						throw Exception("API","Unknown command or action","UNKNOWN_COMMAND");
				}
				catch (Exception &e)
				{
					Logger::Log(LOG_DEBUG,"Unexpected (caught) exception in context "+e.context+" : "+e.error);
					
					QueryResponse response(s);
					response.SetError(e.error);
					response.SetErrorCode(e.code);
					response.SendResponse();
					
					continue;
				}
				
				Logger::Log(LOG_DEBUG,"API : Successfully called, sending response");
				
				// Apply XPath filter if requested
				string xpath = saxh.GetRootAttribute("xpathfilter","");
				if(xpath!="")
				{
					unique_ptr<DOMXPathResult> res(response.GetDOM()->evaluate(xpath,response.GetDOM()->getDocumentElement(),DOMXPathResult::FIRST_RESULT_TYPE));
					
					QueryResponse xpath_response(s);
					xpath_response.GetDOM()->ImportXPathResult(res.get(),xpath_response.GetDOM()->getDocumentElement());
					xpath_response.SendResponse();
				}
				else
					response.SendResponse();
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
