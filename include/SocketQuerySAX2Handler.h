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

#include <string>
#include <map>
#include <vector>

class SocketQuerySAX2Handler : public DefaultHandler {
	
	private:
		std::string group;
		std::map<std::string,std::string> root_attributes;
		
		int query_type;
		int query_options;
		int workflow_id;
		int task_pid;
		int wait_timeout;
		char *workflow_name;
		char *workflow_host;
		char *workflow_user;
		char *file_name;
		char *file_data;
		bool ready;
		
		int level;
		
		WorkflowParameters params;
		std::vector<std::string> inputs;
		
	public:
		static const int QUERY_WORKFLOW_LAUNCH = 1;
		static const int QUERY_WORKFLOW_MIGRATE = 2;
		static const int QUERY_WORKFLOW_INFO = 3;
		static const int QUERY_QUEUE_STATS = 4;
		static const int QUERY_GLOBAL_STATS = 5;
		static const int QUERY_STATUS_WORKFLOWS = 6;
		static const int QUERY_STATUS_SCHEDULER = 7;
		static const int QUERY_STATUS_CONFIGURATION = 8;
		static const int QUERY_WORKFLOW_CANCEL = 9;
		static const int QUERY_WORKFLOW_WAIT = 10;
		static const int QUERY_WORKFLOW_KILLTASK = 11;
		static const int QUERY_CONTROL_RELOAD = 12;
		static const int QUERY_CONTROL_RETRY = 13;
		static const int QUERY_CONTROL_SYNCTASKS = 14;
		static const int QUERY_CONTROL_SYNCNOTIFICATIONS = 15;
		static const int QUERY_NOTIFICATION_PUT = 16;
		static const int QUERY_NOTIFICATION_REM = 17;
		static const int QUERY_NOTIFICATION_PUTCONF = 18;
		static const int QUERY_NOTIFICATION_REMCONF = 19;
		static const int QUERY_NOTIFICATION_GETCONF = 20;
		static const int QUERY_TASK_GET = 21;
		static const int QUERY_TASK_PUT = 22;
		static const int QUERY_TASK_REM = 23;
		
		static const int RESET_GLOBAL_STATS = 24;
		static const int PING = 25;
		
		static const int QUERY_OPTION_MODE_SYNCHRONOUS = 1;
		static const int QUERY_OPTION_MODE_ASYNCHRONOUS = 2;
		
		SocketQuerySAX2Handler();
		~SocketQuerySAX2Handler();
		
		void startElement( const XMLCh* const uri, const XMLCh* const localname, const XMLCh* const qname, const Attributes&  attrs );
		void endElement (const XMLCh *const uri, const XMLCh *const localname, const XMLCh *const qname);
		void endDocument();
		
		bool IsReady() { return ready; }
		const std::string &GetQueryGroup() { return group; }
		const std::map<std::string,std::string> &GetRootAttributes() { return root_attributes; }
		int GetQueryType() { return query_type; }
		int GetQueryOptions() { return query_options; }
		const char *GetWorkflowName() { return workflow_name; }
		const char *GetWorkflowHost() { return workflow_host; }
		const char *GetWorkflowUser() { return workflow_user; }
		const char *GetFileName() { return file_name; }
		const char *GetFileData() { return file_data; }
		int GetWorkflowId() { return workflow_id; }
		int GetTaskPID() { return task_pid; }
		int GetWaitTimeout() { return wait_timeout; }
		WorkflowParameters *GetWorkflowParameters() {return &params;};
		const std::vector<std::string> &GetInputs() { return inputs; }
};
