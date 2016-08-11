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
#include <DB.h>
#include <Exception.h>
#include <WorkflowInstance.h>
#include <QueuePool.h>
#include <Retrier.h>
#include <Statistics.h>
#include <WorkflowScheduler.h>
#include <NetworkInputSource.h>
#include <WorkflowInstances.h>
#include <Logger.h>
#include <Configuration.h>
#include <Sockets.h>
#include <Notification.h>
#include <QueryResponse.h>
#include <QueryHandlers.h>
#include <Task.h>
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
	
	char buf[4096];
	int read_size;
	
	Statistics *stats = Statistics::GetInstance();
	stats->IncAcceptedConnections();
	
	SAX2XMLReader *parser = 0;
	SocketQuerySAX2Handler* saxh = 0;
	NetworkInputSource *source = 0;
	
	// Init mysql library
	mysql_thread_init();
	
	QueryResponse response(s);
	
	try
	{
		parser = XMLReaderFactory::createXMLReader();
		parser->setFeature(XMLUni::fgSAX2CoreValidation, true);
		parser->setFeature(XMLUni::fgSAX2CoreNameSpaces, true);
		
		XMLSize_t lowWaterMark = 0;
		parser->setProperty(XMLUni::fgXercesLowWaterMark, &lowWaterMark);
		
		saxh = new SocketQuerySAX2Handler();
		parser->setContentHandler(saxh);
		parser->setErrorHandler(saxh);
		
		// Create a progressive scan token
		XMLPScanToken token;
		NetworkInputSource source(s);
		
		try
		{
			if (!parser->parseFirst(source, token))
			{
				Logger::Log(LOG_ERR,"parseFirst failed");
				throw Exception("core","parseFirst failed");
			}
			
			bool gotMore = true;
			while (gotMore && !saxh->IsReady()) {
				gotMore = parser->parseNext(token);
			}
		}
		catch (const SAXParseException& toCatch)
		{
			char *message = XMLString::transcode(toCatch.getMessage());
			Logger::Log(LOG_WARNING,"Invalid query XML structure : %s",message);
			XMLString::release(&message);
			
			throw Exception("Query XML parsing","Invalid query XML structure");
		}
		catch(Exception &e)
		{
			stats->IncInputErrors();
			throw e;
		}
		catch(int e)  // int exception thrown to indicate that we have received a complete XML (usual case)
		{
			; // nothing to do, just let the code roll
		}
		catch (...)
		{
			stats->IncInputErrors();
			throw Exception("Workflow XML parsing","Unexpected error trying to parse workflow XML");
		}
		
		if(!QueryHandlers::GetInstance()->HandleQuery(saxh->GetQueryGroup(),saxh, &response))
			response.SetError("Unknown command or action");
		
		response.SendResponse();
	}
	catch (Exception &e)
	{
		Logger::Log(LOG_WARNING,"Unexpected exception in context %s : %s\n",e.context,e.error);
		
		response.SetError(e.error);
		response.SendResponse();
		
		if (parser!=0)
			delete parser;
		if (saxh!=0)
			delete saxh;
		if (source)
			delete source;
		
		Sockets::GetInstance()->UnregisterSocket(s);
		
		mysql_thread_end();
		
		return 0;
		
	}
	
	if (parser!=0)
		delete parser;
	if (saxh!=0)
		delete saxh;
	if (source)
		delete source;
	
	Sockets::GetInstance()->UnregisterSocket(s);
	mysql_thread_end();
	
	return 0;
}
