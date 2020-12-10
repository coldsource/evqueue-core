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
#include <Crypto/sha1.h>
#include <DOM/DOMDocument.h>
#include <Configuration/ConfigurationReader.h>
#include <Configuration/Configuration.h>

#include <map>
#include <string>
#include <iomanip>
#include <sstream>
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
		// Parse cmdline parameters
		bool format = true;

		Configuration config({{"api.connect","tcp://localhost:5000"}, {"api.user", ""}, {"api.password", ""}});
		
		// Try to read config from home directory
		char *home = getenv("HOME");
		if(home)
		{
			string path = home;
			path += "/.evqueue.api.conf";
			
			struct stat file_stats;
			if(stat(path.c_str(), &file_stats)==0)
				ConfigurationReader::Read(path.c_str(), &config);
		}

		int cur;
		for(cur=1;cur<argc;cur++)
		{
			if(strcmp(argv[cur],"--connect")==0)
			{
				if(cur+1>=argc)
					usage();
				
				config.Set("api.connect", argv[cur+1]);
				cur++;
			}
			else if(strcmp(argv[cur],"--user")==0)
			{
				if(cur+1>=argc)
					usage();
				
				config.Set("api.user", argv[cur+1]);
				cur++;
			}
			else if(strcmp(argv[cur],"--password")==0)
			{
				if(cur+1>=argc)
					usage();
				
				config.Set("api.password", argv[cur+1]);
				cur++;
			}
			else if(strcmp(argv[cur],"--noformat")==0)
				format = false;
			else if(strcmp(argv[cur],"--")==0)
			{
				cur++;
				break;
			}
			else
				break;
		}

		string connection_str = config.Get("api.connect");
		string user = config.Get("api.user");
		string password = config.Get("api.password");

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
