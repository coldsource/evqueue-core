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
			Logger::Log(LOG_WARNING,"Invalid workflow XML structure : %s",message);
			XMLString::release(&message);
			throw (void *)0;
		}
		catch(Exception &e)
		{
			stats->IncInputErrors();
			Logger::Log(LOG_WARNING,"[ %s ] %s",e.context,e.error);
			throw (void *)0;
		}
		catch(int e)  // int exception thrown to indicate that we have received a complete XML (usual case)
		{
			; // nothing to do, just let the code roll
		}
		catch (...)
		{
			stats->IncInputErrors();
			Logger::Log(LOG_WARNING,"Unexpected error trying to parse workflow XML");
			throw (void *)0;
		}
		
		// we have a request to launch a new workflow instance
		if ( saxh->GetQueryType() == SocketQuerySAX2Handler::PING)
		{
			send(s,"<pong />",8,0);
			throw (void *)0;
		}
		else if ( saxh->GetQueryType() == SocketQuerySAX2Handler::QUERY_WORKFLOW_LAUNCH)
		{
			const char *workflow_name = saxh->GetWorkflowName();
			WorkflowInstance *wi;
			bool workflow_terminated;
			
			stats->IncWorkflowQueries();
			
			try
			{
				wi = new WorkflowInstance(workflow_name,saxh->GetWorkflowParameters(),0,saxh->GetWorkflowHost(),saxh->GetWorkflowUser());
			}
			catch(Exception &e)
			{
				stats->IncWorkflowExceptions();
				
				send(s,"<return status='KO' error=\"",27,0);
				send(s,e.error,strlen(e.error),0);
				send(s,"\" />",4,0);
				
				Logger::Log(LOG_WARNING,"Unexpected exception trying to instanciate workflow '%s' : [ %s ] %s",workflow_name,e.context,e.error);
				throw (void *)0;
			}
			
			wi->Start(&workflow_terminated);
			
			Logger::Log(LOG_NOTICE,"[WID %d] Instanciated from %s:%d",wi->GetInstanceID(),remote_addr_str,remote_port);
			
			if(saxh->GetQueryOptions()==SocketQuerySAX2Handler::QUERY_OPTION_MODE_SYNCHRONOUS)
				WorkflowInstances::GetInstance()->Wait(wi->GetInstanceID());
			
			sprintf(buf,"<return status='OK' workflow-instance-id='%d' />",wi->GetInstanceID());
			send(s,buf,strlen(buf),0);
			
			if(workflow_terminated)
				delete wi; // This can happen on empty workflows or when dynamic errors occur in workflow (eg unknown queue for a task)
			
			throw (void *)0;
		
		// we have a query to find out information about a given workflow instance
		}
		else if ( saxh->GetQueryType() == SocketQuerySAX2Handler::QUERY_WORKFLOW_INFO)
		{
			int workflow_instance_id = saxh->GetWorkflowId();
			DB db;
			
			stats->IncWorkflowStatusQueries();
			
			WorkflowInstances *wfi = WorkflowInstances::GetInstance();
			if(!wfi->SendStatus(s,workflow_instance_id))
			{
				// Workflow is not executing, lookup in database
				db.QueryPrintf("SELECT workflow_instance_savepoint FROM t_workflow_instance WHERE workflow_instance_id=%i",&workflow_instance_id);
				if(db.FetchRow())
					send(s,db.GetField(0),strlen(db.GetField(0)),0);
				else
					send(s,"<return status='KO' error='UNKNOWN-WORKFLOW-INSTANCE' />",56,0);
			}
			
			throw (void *)0;
		}
		else if ( saxh->GetQueryType() == SocketQuerySAX2Handler::QUERY_WORKFLOW_CANCEL)
		{
			unsigned int workflow_instance_id = saxh->GetWorkflowId();
			
			stats->IncWorkflowCancelQueries();
			
			// Prevent workflow instance from instanciating new tasks
			if(!WorkflowInstances::GetInstance()->Cancel(workflow_instance_id))
			{
				send(s,"<return status='KO' error='UNKNOWN-WORKFLOW-INSTANCE' />",56,0);
				
				throw (void *)0;
			}
			
			// Flush retrier
			Retrier::GetInstance()->FlushWorkflowInstance(workflow_instance_id);
			
			// Cancel currently queued tasks
			QueuePool::GetInstance()->CancelTasks(workflow_instance_id);
			
			send(s,"<return status='OK' />",22,0);
			
			throw (void *)0;
		}
		else if(saxh->GetQueryType()==SocketQuerySAX2Handler::QUERY_WORKFLOW_WAIT)
		{
			unsigned int workflow_instance_id = saxh->GetWorkflowId();
			
			if(!WorkflowInstances::GetInstance()->Wait(workflow_instance_id))
			{
				send(s,"<return status='KO' error='UNKNOWN-WORKFLOW-INSTANCE' />",56,0);
				
				throw (void *)0;
			}
			
			send(s,"<return status='OK' />",22,0);
			
			throw (void *)0;
		}
		else if(saxh->GetQueryType()==SocketQuerySAX2Handler::QUERY_WORKFLOW_KILLTASK)
		{
			unsigned int workflow_instance_id = saxh->GetWorkflowId();
			int task_pid = saxh->GetTaskPID();
			
			if(!WorkflowInstances::GetInstance()->KillTask(workflow_instance_id,task_pid))
			{
				send(s,"<return status='KO' error='UNKNOWN-WORKFLOW-INSTANCE' />",56,0);
				
				throw (void *)0;
			}
			
			send(s,"<return status='OK' />",22,0);
			
			throw (void *)0;
		}
		else if(saxh->GetQueryType()==SocketQuerySAX2Handler::QUERY_NOTIFICATION_PUT)
		{	
			try
			{
				Notification::PutFile(saxh->GetFileName(),saxh->GetFileData());
				send(s,"<return status='OK' />",22,0);
			}
			catch(Exception &e)
			{
				send(s,"<return status='KO' error=\"",27,0);
				send(s,e.error,strlen(e.error),0);
				send(s,"\" />",4,0);
			}
			
			throw (void *)0;
		}
		else if(saxh->GetQueryType()==SocketQuerySAX2Handler::QUERY_NOTIFICATION_PUTCONF)
		{	
			try
			{
				Notification::PutFileConf(saxh->GetFileName(),saxh->GetFileData());
				send(s,"<return status='OK' />",22,0);
			}
			catch(Exception &e)
			{
				send(s,"<return status='KO' error=\"",27,0);
				send(s,e.error,strlen(e.error),0);
				send(s,"\" />",4,0);
			}
			
			throw (void *)0;
		}
		else if(saxh->GetQueryType()==SocketQuerySAX2Handler::QUERY_NOTIFICATION_GETCONF)
		{	
			try
			{
				string file_content;
				Notification::GetFileConf(saxh->GetFileName(),file_content);
				send(s,"<return status='OK' data='",26,0);
				send(s,file_content.c_str(),file_content.length(),0);
				send(s,"' />",4,0);
			}
			catch(Exception &e)
			{
				send(s,"<return status='KO' error=\"",27,0);
				send(s,e.error,strlen(e.error),0);
				send(s,"\" />",4,0);
			}
			
			throw (void *)0;
		}
		else if(saxh->GetQueryType()==SocketQuerySAX2Handler::QUERY_NOTIFICATION_REM)
		{	
			try
			{
				Notification::RemoveFile(saxh->GetFileName());
				send(s,"<return status='OK' />",22,0);
			}
			catch(Exception &e)
			{
				send(s,"<return status='KO' error=\"",27,0);
				send(s,e.error,strlen(e.error),0);
				send(s,"\" />",4,0);
			}
			
			throw (void *)0;
		}
		else if(saxh->GetQueryType()==SocketQuerySAX2Handler::QUERY_NOTIFICATION_REMCONF)
		{	
			try
			{
				Notification::RemoveFileConf(saxh->GetFileName());
				send(s,"<return status='OK' />",22,0);
			}
			catch(Exception &e)
			{
				send(s,"<return status='KO' error=\"",27,0);
				send(s,e.error,strlen(e.error),0);
				send(s,"\" />",4,0);
			}
			
			throw (void *)0;
		}
		else if(saxh->GetQueryType()==SocketQuerySAX2Handler::QUERY_QUEUE_STATS)
		{
			stats->IncStatisticsQueries();
			
			QueuePool *qp = QueuePool::GetInstance();
			qp->SendStatistics(s);
			throw (void *)0;
		}
		else if(saxh->GetQueryType()==SocketQuerySAX2Handler::QUERY_GLOBAL_STATS)
		{
			stats->IncStatisticsQueries();
			
			stats->SendGlobalStatistics(s);
			throw (void *)0;
		}
		else if(saxh->GetQueryType()==SocketQuerySAX2Handler::RESET_GLOBAL_STATS)
		{
			stats->ResetGlobalStatistics();
			
			send(s,"<return status='OK' />",22,0);
			
			throw (void *)0;
		}
		else if(saxh->GetQueryType()==SocketQuerySAX2Handler::QUERY_STATUS_WORKFLOWS)
		{
			stats->IncStatisticsQueries();
			
			WorkflowInstances *workflow_instances = WorkflowInstances::GetInstance();
			workflow_instances->SendStatus(s);
			throw (void *)0;
		}
		else if(saxh->GetQueryType()==SocketQuerySAX2Handler::QUERY_STATUS_SCHEDULER)
		{
			stats->IncStatisticsQueries();
			
			WorkflowScheduler *scheduler = WorkflowScheduler::GetInstance();
			scheduler->SendStatus(s);
			throw (void *)0;
		}
		else if(saxh->GetQueryType()==SocketQuerySAX2Handler::QUERY_STATUS_CONFIGURATION)
		{
			stats->IncStatisticsQueries();
			
			Configuration *config = Configuration::GetInstance();
			config->SendConfiguration(s);
			throw (void *)0;
		}
		else if(saxh->GetQueryType()==SocketQuerySAX2Handler::QUERY_CONTROL_RELOAD)
		{
			tools_config_reload();
			send(s,"<return status='OK' />",22,0);
			throw (void *)0;
		}
		else if(saxh->GetQueryType()==SocketQuerySAX2Handler::QUERY_CONTROL_RETRY)
		{
			tools_flush_retrier();
			send(s,"<return status='OK' />",22,0);
			throw (void *)0;
		}
	}
	catch (void *retval)
	{
		if (parser!=0)
			delete parser;
		if (saxh!=0)
			delete saxh;
		if (source)
			delete source;
		
		Sockets::GetInstance()->UnregisterSocket(s);
		
		mysql_thread_end();
		
		return retval;
		
	}
}
