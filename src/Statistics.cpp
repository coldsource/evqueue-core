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
#include <ActiveConnections.h>
#include <SocketQuerySAX2Handler.h>
#include <QueryResponse.h>
#include <Exception.h>
#include <QueuePool.h>
#include <User.h>
#include <DOMDocument.h>

#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>

Statistics *Statistics::instance = 0;

using namespace std;

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
	DOMDocument *xmldoc = response->GetDOM();
	
	DOMElement statistics_node = xmldoc->createElement("statistics");
	xmldoc->getDocumentElement().appendChild(statistics_node);
	statistics_node.setAttribute("accepted_connections",to_string(accepted_connections));
	statistics_node.setAttribute("current_connections",to_string(ActiveConnections::GetInstance()->GetNumber()));
	statistics_node.setAttribute("input_errors",to_string(input_errors));
	statistics_node.setAttribute("workflow_queries",to_string(workflow_queries));
	statistics_node.setAttribute("workflow_status_queries",to_string(workflow_status_queries));
	statistics_node.setAttribute("workflow_cancel_queries",to_string(workflow_cancel_queries));
	statistics_node.setAttribute("statistics_queries",to_string(statistics_queries));
	statistics_node.setAttribute("workflow_exceptions",to_string(workflow_exceptions));
	statistics_node.setAttribute("workflow_instance_launched",to_string(workflow_instance_launched));
	statistics_node.setAttribute("workflow_instance_executing",to_string(workflow_instance_executing));
	statistics_node.setAttribute("workflow_instance_errors",to_string(workflow_instance_errors));
	statistics_node.setAttribute("waiting_threads",to_string(waiting_threads));
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

bool Statistics::HandleQuery(const User &user, SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	Statistics *stats = Statistics::GetInstance();
	
	const string action = saxh->GetRootAttribute("action");
	
	if(action=="query")
	{
		const string type = saxh->GetRootAttribute("type");
		
		if(type=="global")
		{
			stats->IncStatisticsQueries();
			
			stats->SendGlobalStatistics(response);
			
			return true;
		}
		else if(type=="queue")
		{
			stats->IncStatisticsQueries();
				
			QueuePool *qp = QueuePool::GetInstance();
			qp->SendStatistics(response);
			
			return true;
		}
		else
			throw Exception("Statistics","Unknown statistics type");
	}
	else if(action=="reset")
	{
		if(!user.IsAdmin())
			User::InsufficientRights();
		
		const string type = saxh->GetRootAttribute("type");
		
		if(type=="global")
		{
			stats->ResetGlobalStatistics();
			
			return true;
		}
		else
			throw Exception("Statistics","Unknown statistics type for this action");
	}
	
	return false;
}