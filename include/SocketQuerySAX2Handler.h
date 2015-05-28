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

#include <global.h>
#include <WorkflowParameters.h>
#include <xercesc/sax2/DefaultHandler.hpp>

using namespace xercesc;

class SocketQuerySAX2Handler : public DefaultHandler {
	
	private:
		int query_type;
		int query_options;
		int workflow_id;
		int task_pid;
		char *workflow_name;
		char *workflow_host;
		char *workflow_user;
		char *file_name;
		char *file_data;
		bool ready;
		
		int level;
		
		WorkflowParameters params;
		
	public:
		static const int QUERY_WORKFLOW_LAUNCH = 1;
		static const int QUERY_WORKFLOW_INFO = 2;
		static const int QUERY_QUEUE_STATS = 3;
		static const int QUERY_GLOBAL_STATS = 4;
		static const int QUERY_WORKFLOWS_STATUS = 5;
		static const int QUERY_SCHEDULER_STATUS = 6;
		static const int QUERY_WORKFLOW_CANCEL = 7;
		static const int QUERY_WORKFLOW_WAIT = 8;
		static const int QUERY_WORKFLOW_KILLTASK = 9;
		static const int QUERY_CONTROL_RELOAD = 10;
		static const int QUERY_NOTIFICATION_PUT = 11;
		static const int QUERY_NOTIFICATION_REM = 12;
		
		static const int RESET_GLOBAL_STATS = 13;
		static const int PING = 14;
		
		static const int QUERY_OPTION_MODE_SYNCHRONOUS = 1;
		static const int QUERY_OPTION_MODE_ASYNCHRONOUS = 2;
		
		SocketQuerySAX2Handler();
		~SocketQuerySAX2Handler();
		
		void startElement( const XMLCh* const uri, const XMLCh* const localname, const XMLCh* const qname, const Attributes&  attrs );
		void endElement (const XMLCh *const uri, const XMLCh *const localname, const XMLCh *const qname);
		void endDocument();
		
		bool IsReady() { return ready; }
		int GetQueryType() { return query_type; }
		int GetQueryOptions() { return query_options; }
		const char *GetWorkflowName() { return workflow_name; }
		const char *GetWorkflowHost() { return workflow_host; }
		const char *GetWorkflowUser() { return workflow_user; }
		const char *GetFileName() { return file_name; }
		const char *GetFileData() { return file_data; }
		int GetWorkflowId() { return workflow_id; }
		int GetTaskPID() { return task_pid; }
		WorkflowParameters *GetWorkflowParameters() {return &params;};
};
