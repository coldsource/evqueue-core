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

#include <API/ClientBase.h>
#include <API/XMLResponse.h>
#include <Exception/Exception.h>
#include <XML/XMLString.h>
#include <Configuration/ConfigurationReader.h>
#include <Configuration/Configuration.h>

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

static void usage()
{
	fprintf(stderr,"Usage : evqueue_wfmanager --launch <wf name> --<parameter name> <parameter value>\n");
	fprintf(stderr,"  --connect tcp://<hostname>[:<port>] | unix://<path>\n");
	fprintf(stderr,"  --user <username>\n");
	fprintf(stderr,"  --password <password>\n");
	fprintf(stderr,"  --mode [synchronous|asynchronous]\n");
	fprintf(stderr,"  --timeout <timeout>\n");
	fprintf(stderr,"  --comment <comment>\n");
	exit(-1);
}

int main(int argc, char  **argv)
{
	int exit_status = 0;
	
	try
	{
		// Default config
		Configuration config({{"api.connect","tcp://localhost:5000"}, {"api.user", ""}, {"api.password", ""}});
		
		// Try to read from default paths
		ConfigurationReader::ReadDefaultPaths("evqueue.api.conf", &config);
		
		// Override with command line
		int cmd_cur = ConfigurationReader::ReadCommandLine(argc, argv, {"connect", "user", "password", "bool:noformat"}, "api", &config);
		if(cmd_cur==-1)
			usage();

		string connection_str = config.Get("api.connect");
		string user = config.Get("api.user");
		string password = config.Get("api.password");
		
		// Parse cmdline parameters
		char *workflow_name = 0;
		char *workflow_mode = 0;
		char *workflow_comment = 0;
		char *timeout = 0;
		
		int cur;
		for(cur=cmd_cur;cur<argc;cur++)
		{
			if(strcmp(argv[cur],"--launch")==0)
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
			else if(strcmp(argv[cur],"--comment")==0)
			{
				if(cur+1>=argc)
				{
					fprintf(stderr,"Missing comment : --mode <comment>\n");
					return -1;
				}
				
				workflow_comment = argv[cur+1];
				cur++;
			}
			else
			{
				fprintf(stderr,"Unknown parameter name : %s\n",argv[cur]);
				return -1;
			}
		}
		
		if(!workflow_name)
			usage();
		
		// If we received a password, compute sha1 of it as a real password
		if(password.length())
			password = ClientBase::HashPassword(password);
		
		// Build workflow XML
		xercesc::XMLPlatformUtils::Initialize();
		
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
			if(workflow_comment)
				workflow_node.setAttribute("comment",workflow_comment);
			
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
				
				XMLResponse *response = client.GetResponseHandler();
				string wfid = response->GetRootAttribute("workflow-instance-id");
				printf("Launched instance %s\n",wfid.c_str());
				
			}
			catch(Exception &e)
			{
				fprintf(stderr,"Exception in client : %s\n",e.error.c_str());
				exit_status = -1;
			}
		}
	}
	catch(Exception &e)
	{
		fprintf(stderr,"%s",e.error.c_str());
		if(e.code!="")
			fprintf(stderr," (%s)",e.code.c_str());
		fprintf(stderr,"\n");
		exit_status = -1;
	}
	
	xercesc::XMLPlatformUtils::Terminate();
	
	return exit_status;
}
