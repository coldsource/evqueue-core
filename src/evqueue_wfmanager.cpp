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

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <netdb.h>

#include <xqilla/xqilla-dom3.hpp>
#include <xercesc/dom/DOM.hpp>

using namespace xercesc;

int connect_socket(const char *connection_str);

int main(int argc, char  **argv)
{
	// Parse cmdline parameters
	char *workflow_name = 0;
	char *workflow_mode = 0;
	char *timeout = 0;
	
	const char *connection_str = "tcp://localhost:5000";
	
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
		fprintf(stderr,"  --mode [synchronous|asynchronous]\n");
		fprintf(stderr,"  --timeout <timeout>\n");
		return -1;
	}
	
	// Build workflow XML
	XQillaPlatformUtils::initialize();
	
	DOMImplementation *xqillaImplementation = DOMImplementationRegistry::getDOMImplementation(X("XPath2 3.0"));
	DOMDocument *xmldoc = xqillaImplementation->createDocument();
	
	DOMElement *workflow_node = xmldoc->createElement(X("workflow"));
	xmldoc->appendChild(workflow_node);
	
	workflow_node->setAttribute(X("name"),X(workflow_name));
	if(workflow_mode)
		workflow_node->setAttribute(X("mode"),X(workflow_mode));
	if(timeout)
		workflow_node->setAttribute(X("timeout"),X(timeout));
	
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
			
			DOMElement *parameter_node = xmldoc->createElement(X("parameter"));
			parameter_node->setAttribute(X("name"),X(parameter_name));
			parameter_node->setAttribute(X("value"),X(parameter_value));
			workflow_node->appendChild(parameter_node);
		}
	}
	
	DOMLSSerializer *serializer = xqillaImplementation->createLSSerializer();
	XMLCh *workflow_xml = serializer->writeToString(workflow_node);
	char *workflow_xml_c = XMLString::transcode(workflow_xml);
	
	// Send launch command to evQueue
	int s = connect_socket(connection_str);
	send(s,workflow_xml_c,strlen(workflow_xml_c),0);
	
	XMLString::release(&workflow_xml);
	XMLString::release(&workflow_xml_c);
	serializer->release();
	xmldoc->release();
	
	// Fetch evQueue response
	char response[1024];
	int re, response_len = 0;
	while((re = recv(s,response+response_len,1023-response_len,0))>0)
		response_len += re;
	response[response_len] = '\0';
	
	// Parse response
	int return_code = 0;
	
	DOMLSParser *parser = xqillaImplementation->createLSParser(DOMImplementationLS::MODE_SYNCHRONOUS,0);
	
	
	XMLCh *xml;
	xml = XMLString::transcode(response);
	
	DOMLSInput *input = xqillaImplementation->createLSInput();
	input->setStringData(xml);
	DOMDocument *xmldoc_response = parser->parse(input);
	
	input->release();
	XMLString::release(&xml);
	
	DOMElement *root = xmldoc_response->getDocumentElement();
	const XMLCh *response_status = root->getAttribute(X("status"));
	const XMLCh *response_wfid = root->getAttribute(X("workflow-instance-id"));
	
	if(XMLString::compareString(response_status,X("OK"))==0)
	{
		return_code = 0;
		
		char *response_wfid_c = XMLString::transcode(response_wfid);
		printf("Launched instance %s\n",response_wfid_c);
		XMLString::release(&response_wfid_c);
	}
	else
	{
		return_code = 1;
		
		const XMLCh *response_error = root->getAttribute(X("error"));
		if(response_error)
		{
			char *response_error_c = XMLString::transcode(response_error);
			fprintf(stderr,"evQueue returned error : %s\n",response_error_c);
			XMLString::release(&response_error_c);
		}
	}
	
	parser->release();
	
	XQillaPlatformUtils::terminate();
	
	return return_code;
}

int connect_socket(const char *connection_str_const)
{
	int s;
	
	char *connection_str = new char[strlen(connection_str_const)+1];
	strcpy(connection_str,connection_str_const);
	
	if(strncmp(connection_str,"tcp://",6)==0)
	{
		char *hostname = connection_str+6;
		if(hostname[0]=='\0')
		{
			delete[] connection_str;
			fprintf(stderr,"Missing hostname, expected format : tcp://<hostname>[:<port>]\n");
			exit(-1);
		}
		
		int port = 5000;
		char *port_str = strstr(hostname,":");
		if(!port_str)
			port = 5000;
		else
		{
			port_str[0] = '\0';
			port_str++;
			
			if(port_str[0]=='\0')
			{
				delete[] connection_str;
				fprintf(stderr,"Missing hostname, expected format : tcp://<hostname>[:<port>]\n");
				exit(-1);
			}
			
			port = atoi(port_str);
		}
		
		s =  socket(AF_INET, SOCK_STREAM, 0);
		
		struct hostent *he;
		he = gethostbyname(hostname);
		if(!he)
		{
			delete[] connection_str;
			fprintf(stderr,"Unknown host : %s\n",hostname);
			exit(-1);
		}
		
		struct sockaddr_in serv_addr;
		serv_addr.sin_family = AF_INET;
		memcpy(&serv_addr.sin_addr, he->h_addr_list[0], he->h_length);
		serv_addr.sin_port = htons(port);
		
		if(connect(s, (struct sockaddr *)&serv_addr, sizeof(serv_addr))!=0)
		{
			delete[] connection_str;
			fprintf(stderr,"Unable to connect\n");
			exit(-1);
		}
	}
	else if(strncmp(connection_str,"unix://",7)==0)
	{
		char *path = connection_str+7;
		if(path[0]=='\0')
		{
			delete[] connection_str;
			fprintf(stderr,"Missing path, expected format : unix://<path>\n");
			exit(-1);
		}
		
		int path_len = strlen(path);
		if(path_len>=sizeof(sockaddr_un::sun_path))
		{
			delete[] connection_str;
			fprintf(stderr,"Unix socket path is too long\n");
			exit(-1);
		}
		
		s =  socket(AF_UNIX, SOCK_STREAM, 0);
		
		struct sockaddr_un serv_addr;
		serv_addr.sun_family = AF_UNIX;
		strcpy(serv_addr.sun_path,path);
		
		if(connect(s, (struct sockaddr *)&serv_addr, sizeof(serv_addr))!=0)
		{
			delete[] connection_str;
			fprintf(stderr,"Unable to connect\n");
			exit(-1);
		}
	}
	else
	{
		delete[] connection_str;
		fprintf(stderr,"Invalid connection string format. Expected :\n  tcp://<hostname>[:<port>]\nunix://<path>\n");
		exit(-1);
	}
	
	delete[] connection_str;
	
	return s;
}