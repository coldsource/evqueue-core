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

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <API/ClientBase.h>
#include <Exception/Exception.h>
#include <XML/XMLFormatter.h>
#include <XML/XMLString.h>
#include <DOM/DOMDocument.h>
#include <Configuration/ConfigurationReader.h>
#include <Configuration/Configuration.h>

#include <map>
#include <string>
#include <algorithm>

using namespace std;

static void usage()
{
	fprintf(stderr,"Usage : evqueue_api --<group> --<action> [--<parameter name> <parameter value>]*\n");
	fprintf(stderr,"  --connect <cnx string>\n");
	fprintf(stderr,"  --user <username>\n");
	fprintf(stderr,"  --password <password>\n");
	fprintf(stderr,"  --noformat\n");
	exit(-1);
}

int main(int argc, char  **argv)
{
	int exit_status = 0;
	
	try
	{
		// Default config
		Configuration config({{"api.connect","tcp://localhost:5000"}, {"api.user", ""}, {"api.password", ""}, {"api.noformat", "no"}});
		
		// Try to read from default paths
		ConfigurationReader::ReadDefaultPaths("evqueue.api.conf", &config);
		
		// Override with command line
		int cur = ConfigurationReader::ReadCommandLine(argc, argv, {"connect", "user", "password", "bool:noformat"}, "api", &config);
		if(cur==-1)
			usage();

		string connection_str = config.Get("api.connect");
		string user = config.Get("api.user");
		string password = config.Get("api.password");
		bool format = !config.GetBool("api.noformat");

		// If we received a password, compute sha1 of it as a real password
		if(password.length())
			password = ClientBase::HashPassword(password);

		// Read group and action
		if(cur+1>=argc)
			usage();

		if(strncmp(argv[cur],"--",2)!=0)
			usage();
		const char *group = argv[cur]+2;

		if(strncmp(argv[cur+1],"--",2)!=0)
			usage();
		const char *action = argv[cur+1]+2;

		cur+=2;

		// Read parameters
		map<string,string> parameters;
		for(;cur<argc;cur+=2)
		{
			if(strncmp(argv[cur],"--",2)!=0)
				usage();
			
			string name = argv[cur];
			if(cur+1>=argc)
				usage();
			
			string value = argv[cur+1];
			
			parameters[name] = value;
		}

		// Build API Query XML
		xercesc::XMLPlatformUtils::Initialize();

		{
			DOMDocument xmldoc;
			DOMElement root_node = xmldoc.createElement(group);
			xmldoc.appendChild(root_node);
			
			root_node.setAttribute("action",action);
			for(auto it=parameters.begin();it!=parameters.end();it++)
			{
				string name = it->first;
				name = name.substr(2);
				replace( name.begin(), name.end(), '-', '_');
				root_node.setAttribute(name,it->second);
			}
				
			string query_xml = xmldoc.Serialize(xmldoc.getDocumentElement());
			
			// Send API command to evQueue
			{
				ClientBase client(connection_str,user,password);
				client.Exec(query_xml);
				
				DOMDocument *xmldoc = client.GetResponseDOM();
				
				string response_xml = xmldoc->Serialize(xmldoc->getDocumentElement());
				
				if(format)
				{
					XMLFormatter formatter(response_xml);
					formatter.Format();
				}
				else
					printf("%s\n",response_xml.c_str());
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
