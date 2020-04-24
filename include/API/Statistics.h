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

#ifndef _STATISTICS_H_
#define _STATISTICS_H_

#include <mutex>

class XMLQuery;
class QueryResponse;
class User;

class Statistics
{
	private:
		static Statistics *instance;
		
		unsigned int accepted_api_connections;
		unsigned int accepted_ws_connections;
		unsigned int api_exceptions;
		unsigned int workflow_queries;
		unsigned int workflow_status_queries;
		unsigned int workflow_cancel_queries;
		unsigned int statistics_queries;
		unsigned int workflow_exceptions;
		unsigned int workflow_instance_launched;
		unsigned int workflow_instance_executing;
		unsigned int workflow_instance_errors;
		unsigned int waiting_threads;
		
		std::mutex lock;
	
	public:
		Statistics(void);
		
		static Statistics *GetInstance(void) { return instance; }
		
		unsigned int GetAcceptedConnections(void);
		void IncAPIAcceptedConnections(void);
		void IncWSAcceptedConnections(void);
		void IncAPIExceptions(void);
		void IncWorkflowQueries(void);
		void IncWorkflowStatusQueries(void);
		void IncWorkflowCancelQueries(void);
		void IncStatisticsQueries(void);
		void IncWorkflowExceptions(void);
		void IncWorkflowInstanceExecuting(void);
		void DecWorkflowInstanceExecuting(void);
		void IncWorkflowInstanceErrors(void);
		void IncWaitingThreads(void);
		void DecWaitingThreads(void);
		
		void SendGlobalStatistics(QueryResponse *response);
		void ResetGlobalStatistics();
		
		static bool HandleQuery(const User &user, XMLQuery *query, QueryResponse *response);
};

#endif
