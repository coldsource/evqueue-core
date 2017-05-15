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

#include <ClientBase.h>
#include <SocketResponseSAX2Handler.h>
#include <Exception.h>
#include <XMLString.h>
#include <sha1.h>

#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <netdb.h>

#include <map>
#include <string>
#include <iomanip>
#include <sstream>

#include <xercesc/dom/DOM.hpp>

using namespace std;

int connect_socket(const char *connection_str);

int main(int argc, char  **argv)
{
	// Parse cmdline parameters
	char *workflow_name = 0;
	char *workflow_mode = 0;
	char *timeout = 0;
	
	// Parse cmdline parameters
	string connection_str = "tcp://localhost:5000";
	string user = "";
	string password = "";
	
	int cur;
	for(cur=1;cur<argc;cur++)
	{
		if(strcmp(argv[cur],"--connect")==0)
		{
			if(cur+1>=argc)
			{
				fprintf(stderr,"Missing connection string : --connect <connection string>\n");
				return -1;
			}
			
			connection_str = argv[cur+1];
			cur++;
		}
		else if(strcmp(argv[cur],"--user")==0)
		{
			if(cur+1>=argc)
			{
				fprintf(stderr,"Missing username string : --user <username>\n");
				return -1;
			}
			
			user = argv[cur+1];
			cur++;
		}
		else if(strcmp(argv[cur],"--password")==0)
		{
			if(cur+1>=argc)
			{
				fprintf(stderr,"Missing password string : --user <password>\n");
				return -1;
			}
			
			password = argv[cur+1];
			cur++;
		}
		else if(strcmp(argv[cur],"--launch")==0)
		{
			if(cur+1>=argc)
			{
				fprintf(stderr,"Missing workflow name : --launch <workflow name>\n");
				return -1;
			}
			
			workflow_name = argv[cur+1];
			cur+=2;
			break;
		}
		else if(strcmp(argv[cur],"--mode")==0)
		{
			if(cur+1>=argc)
			{
				fprintf(stderr,"Missing mode : --mode [synchronous|asynchronous]\n");
				return -1;
			}
			
			workflow_mode = argv[cur+1];
			cur++;
		}
		else if(strcmp(argv[cur],"--timeout")==0)
		{
			if(cur+1>=argc)
			{
				fprintf(stderr,"Missing timeout : --timeout <timeout>\n");
				return -1;
			}
			
			timeout = argv[cur+1];
			cur++;
		}
		else
		{
			fprintf(stderr,"Unknown parameter name : %s\n",argv[cur]);
			return -1;
		}
	}
	
	if(!workflow_name)
	{
		fprintf(stderr,"Usage : evqueue_wfmanager --launch <wf name> --<parameter name> <parameter value>\n");
		fprintf(stderr,"  --connect tcp://<hostname>[:<port>] | unix://<path>\n");
		fprintf(stderr,"  --user <username>\n");
		fprintf(stderr,"  --password <password>\n");
		fprintf(stderr,"  --mode [synchronous|asynchronous]\n");
		fprintf(stderr,"  --timeout <timeout>\n");
		return -1;
	}
	
	// If we received a password, compute sha1 of it as a real password
	if(password.length())
	{
		sha1_ctx ctx;
		char c_hash[20];
		
		sha1_init_ctx(&ctx);
		sha1_process_bytes(password.c_str(),password.length(),&ctx);
		sha1_finish_ctx(&ctx,c_hash);
		
		// Format HEX result
		stringstream sstream;
		sstream << hex;
		for(int i=0;i<20;i++)
			sstream << std::setw(2) << setfill('0') << (int)(c_hash[i]&0xFF);
		password = sstream.str();
	}
	
	// Build workflow XML
	xercesc::XMLPlatformUtils::Initialize();
	
	int exit_status = 0;
	
	{
		DOMDocument xmldoc;
		DOMElement workflow_node = xmldoc.createElement("instance");
		xmldoc.appendChild(workflow_node);
		
		workflow_node.setAttribute("action","launch");
		workflow_node.setAttribute("name",workflow_name);
		if(workflow_mode)
			workflow_node.setAttribute("mode",workflow_mode);
		if(timeout)
			workflow_node.setAttribute("timeout",timeout);
		
		int status = 1;
		char *parameter_name, *parameter_value;
		for(int i=cur;i<argc;i++)
		{
			if(status==1)
			{
				// Parameter name expected
				if(strncmp(argv[i],"--",2)!=0 || strlen(argv[i])<=2)
				{
					fprintf(stderr,"Parameter name expected with format : --<parameter name>\n");
					return -1;
				}
				
				parameter_name = argv[i]+2;
				status = 2;
			}
			else if(status==2)
			{
				// Value expacted
				parameter_value = argv[i];
				status=1;
				
				DOMElement parameter_node = xmldoc.createElement("parameter");
				parameter_node.setAttribute("name",parameter_name);
				parameter_node.setAttribute("value",parameter_value);
				workflow_node.appendChild(parameter_node);
			}
		}
		
		string workflow_xml = xmldoc.Serialize(workflow_node);
		
		// Send launch command to evQueue
		
		try
		{
			ClientBase client(connection_str,user,password);
			client.Exec(workflow_xml);
			
			SocketResponseSAX2Handler *saxh = client.GetResponseHandler();
			string wfid = saxh->GetRootAttribute("workflow-instance-id");
			printf("Launched instance %s\n",wfid.c_str());
			
		}
		catch(Exception &e)
		{
			fprintf(stderr,"Exception in client : %s\n",e.error.c_str());
			exit_status = -1;
		}
	}
	
	xercesc::XMLPlatformUtils::Terminate();
	
	return exit_status;
}