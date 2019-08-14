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

#include <API/ClientBase.h>
#include <Exception/Exception.h>
#include <XML/XMLFormatter.h>
#include <XML/XMLString.h>
#include <Crypto/sha1.h>
#include <DOM/DOMDocument.h>

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
	// Parse cmdline parameters
	string connection_str = "tcp://localhost:5000";
	string user = "";
	string password = "";
	bool format = true;
	
	int cur;
	for(cur=1;cur<argc;cur++)
	{
		if(strcmp(argv[cur],"--connect")==0)
		{
			if(cur+1>=argc)
				usage();
			
			connection_str = argv[cur+1];
			cur++;
		}
		else if(strcmp(argv[cur],"--user")==0)
		{
			if(cur+1>=argc)
				usage();
			
			user = argv[cur+1];
			cur++;
		}
		else if(strcmp(argv[cur],"--password")==0)
		{
			if(cur+1>=argc)
				usage();
			
			password = argv[cur+1];
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
	
	int exit_status = 0;
	
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
		
		try
		{
			ClientBase client(connection_str,user,password);
			client.Exec(query_xml,true);
			
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
		catch(Exception &e)
		{
			fprintf(stderr,"%s",e.error.c_str());
			if(e.code!="")
				fprintf(stderr," (%s)",e.code.c_str());
			fprintf(stderr,"\n");
			exit_status = -1;
		}
	}
	
	xercesc::XMLPlatformUtils::Terminate();
	
	return exit_status;
}
