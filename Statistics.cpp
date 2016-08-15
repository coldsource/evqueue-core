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

#include <Statistics.h>
#include <Sockets.h>
#include <SocketQuerySAX2Handler.h>
#include <QueryResponse.h>
#include <Exception.h>
#include <QueuePool.h>

#include <xqilla/xqilla-dom3.hpp>
#include <xercesc/dom/DOM.hpp>

#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>

Statistics *Statistics::instance = 0;

using namespace xercesc;

Statistics::Statistics(void)
{
	pthread_mutex_init(&lock,NULL);
	
	instance = this;
	
	accepted_connections = 0;
	input_errors = 0;
	workflow_queries = 0;
	workflow_status_queries = 0;
	workflow_cancel_queries = 0;
	statistics_queries = 0;
	workflow_exceptions = 0;
	workflow_instance_launched = 0;
	workflow_instance_executing = 0;
	workflow_instance_errors = 0;
	waiting_threads = 0;
}

unsigned int Statistics::GetAcceptedConnections(void)
{
	pthread_mutex_lock(&lock);
	unsigned int n = accepted_connections;
	pthread_mutex_unlock(&lock);
	
	return n;
}

void Statistics::IncAcceptedConnections(void)
{
	pthread_mutex_lock(&lock);
	accepted_connections++;
	pthread_mutex_unlock(&lock);
}

void Statistics::IncInputErrors(void)
{
	pthread_mutex_lock(&lock);
	input_errors++;
	pthread_mutex_unlock(&lock);
}

void Statistics::IncWorkflowQueries(void)
{
	pthread_mutex_lock(&lock);
	workflow_queries++;
	pthread_mutex_unlock(&lock);
}

void Statistics::IncWorkflowStatusQueries(void)
{
	pthread_mutex_lock(&lock);
	workflow_status_queries++;
	pthread_mutex_unlock(&lock);
}

void Statistics::IncWorkflowCancelQueries(void)
{
	pthread_mutex_lock(&lock);
	workflow_cancel_queries++;
	pthread_mutex_unlock(&lock);
}

void Statistics::IncStatisticsQueries(void)
{
	pthread_mutex_lock(&lock);
	statistics_queries++;
	pthread_mutex_unlock(&lock);
}

void Statistics::IncWorkflowExceptions(void)
{
	pthread_mutex_lock(&lock);
	workflow_exceptions++;
	pthread_mutex_unlock(&lock);
}

void Statistics::IncWorkflowInstanceExecuting(void)
{
	pthread_mutex_lock(&lock);
	workflow_instance_launched++;
	workflow_instance_executing++;
	pthread_mutex_unlock(&lock);
}

void Statistics::DecWorkflowInstanceExecuting(void)
{
	pthread_mutex_lock(&lock);
	if(workflow_instance_executing>0)
		workflow_instance_executing--;
	pthread_mutex_unlock(&lock);
}

void Statistics::IncWorkflowInstanceErrors(void)
{
	pthread_mutex_lock(&lock);
	workflow_instance_errors++;
	pthread_mutex_unlock(&lock);
}

void Statistics::IncWaitingThreads(void)
{
	pthread_mutex_lock(&lock);
	waiting_threads++;
	pthread_mutex_unlock(&lock);
}

void Statistics::DecWaitingThreads(void)
{
	pthread_mutex_lock(&lock);
	waiting_threads--;
	pthread_mutex_unlock(&lock);
}

void Statistics::SendGlobalStatistics(QueryResponse *response)
{
	char buf[16];
	
	DOMDocument *xmldoc = response->GetDOM();
	
	DOMElement *statistics_node = xmldoc->createElement(X("statistics"));
	xmldoc->getDocumentElement()->appendChild(statistics_node);
	
	sprintf(buf,"%d",accepted_connections);
	statistics_node->setAttribute(X("accepted_connections"),X(buf));
	
	sprintf(buf,"%d",Sockets::GetInstance()->GetNumber());
	statistics_node->setAttribute(X("current_connections"),X(buf));
	
	sprintf(buf,"%d",input_errors);
	statistics_node->setAttribute(X("input_errors"),X(buf));
	
	sprintf(buf,"%d",workflow_queries);
	statistics_node->setAttribute(X("workflow_queries"),X(buf));
	
	sprintf(buf,"%d",workflow_status_queries);
	statistics_node->setAttribute(X("workflow_status_queries"),X(buf));
	
	sprintf(buf,"%d",workflow_cancel_queries);
	statistics_node->setAttribute(X("workflow_cancel_queries"),X(buf));
	
	sprintf(buf,"%d",statistics_queries);
	statistics_node->setAttribute(X("statistics_queries"),X(buf));
	
	sprintf(buf,"%d",workflow_exceptions);
	statistics_node->setAttribute(X("workflow_exceptions"),X(buf));
	
	sprintf(buf,"%d",workflow_instance_launched);
	statistics_node->setAttribute(X("workflow_instance_launched"),X(buf));
	
	sprintf(buf,"%d",workflow_instance_executing);
	statistics_node->setAttribute(X("workflow_instance_executing"),X(buf));
	
	sprintf(buf,"%d",workflow_instance_errors);
	statistics_node->setAttribute(X("workflow_instance_errors"),X(buf));
	
	sprintf(buf,"%d",waiting_threads);
	statistics_node->setAttribute(X("waiting_threads"),X(buf));
}

void Statistics::ResetGlobalStatistics()
{
	accepted_connections = 0;
	input_errors = 0;
	workflow_queries = 0;
	workflow_status_queries = 0;
	workflow_cancel_queries = 0;
	statistics_queries = 0;
	workflow_exceptions = 0;
	workflow_instance_launched = 0;
	workflow_instance_executing = 0;
	workflow_instance_errors = 0;
}

bool Statistics::HandleQuery(SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	Statistics *stats = Statistics::GetInstance();
	
	const std::map<std::string,std::string> attrs = saxh->GetRootAttributes();
	
	auto it_action = attrs.find("action");
	if(it_action==attrs.end())
		throw Exception("Statistics","Missing action attribute on node statistics");
	
	if(it_action->second=="query")
	{
		auto it_type = attrs.find("type");
		if(it_type==attrs.end())
			throw Exception("Statistics","Missing type attribute on node statistics");
		
		if(it_type->second=="global")
		{
			stats->IncStatisticsQueries();
			
			stats->SendGlobalStatistics(response);
			
			return true;
		}
		else if(it_type->second=="queue")
		{
			stats->IncStatisticsQueries();
				
			QueuePool *qp = QueuePool::GetInstance();
			qp->SendStatistics(response);
			
			return true;
		}
		else
			throw Exception("Statistics","Unknown statistics type");
	}
	else if(it_action->second=="reset")
	{
		auto it_type = attrs.find("type");
		if(it_type==attrs.end())
			throw Exception("Statistics","Missing type attribute on node statistics");
		
		if(it_type->second=="global")
		{
			stats->ResetGlobalStatistics();
			
			return true;
		}
		else
			throw Exception("Statistics","Unknown statistics type for this action");
	}
	
	return false;
}