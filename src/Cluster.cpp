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

#include <Cluster.h>
#include <Exception.h>
#include <SocketResponseSAX2Handler.h>
#include <SocketSAX2Handler.h>
#include <Sockets.h>
#include <Logger.h>

#include <sstream>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <netdb.h>

using namespace std;

void Cluster::ParseConfiguration(const string &conf)
{
	istringstream split(conf);
	for (std::string each; std::getline(split, each, ','); nodes.push_back(each));
}

void Cluster::ExecuteCommand(const string &command)
{
	Logger::Log(LOG_NOTICE, "Executing cluster command %s",command.c_str());
	
	for(int i=0;i<nodes.size();i++)
		execute_command(nodes.at(i),command);
}

void Cluster::execute_command(const string &cnx_str, const string &command)
{
	// Send command
	int s;
	
	try
	{
		s = connect_socket(cnx_str);
	}
	catch(Exception &e)
	{
		Logger::Log(LOG_WARNING,"Cluster encountered unexpected exception in context %s : %s\n",e.context.c_str(),e.error.c_str());
		
		return;
	}
	
	Sockets::GetInstance()->RegisterSocket(s);
	
	send(s,command.c_str(),command.length(),0);
	
	// Parse response
	try
	{
		SocketResponseSAX2Handler saxh("Cluster");
		
		SocketSAX2Handler socket_sax2_handler(s);
		socket_sax2_handler.HandleQuery(&saxh);
	}
	catch(Exception &e)
	{
		Logger::Log(LOG_WARNING,"Cluster encountered unexpected exception in context %s : %s\n",e.context.c_str(),e.error.c_str());
		
		Sockets::GetInstance()->UnregisterSocket(s);
		
		throw e;
	}
	
	Sockets::GetInstance()->UnregisterSocket(s);
}

int Cluster::connect_socket(const string &connection_str_const)
{
	int s;
	
	string connection_str = connection_str_const;
	
	if(connection_str.substr(0,6)=="tcp://")
	{
		if(connection_str.length()==6)
			throw Exception("connect_socket","Missing hostname, expected format : tcp://<hostname>[:<port>]");
		
		string hostname = connection_str.substr(6,string::npos);
		
		int port;
		size_t port_pos = hostname.find(":");
		if(port_pos==string::npos)
			port = 5000;
		else
		{
			string port_str = hostname.substr(port_pos+1,string::npos);
			hostname = hostname.substr(0,port_pos);
			
			if(port_str.length()==0)
				throw Exception("connect_socket","Missing hostname, expected format : tcp://<hostname>[:<port>]");
			
			try
			{
				port = std::stoi(port_str	);
			}
			catch(...)
			{
				throw Exception("connect_socket","Invalid port number");
			}
		}
		
		s =  socket(AF_INET, SOCK_STREAM, 0);
		
		struct hostent *he;
		he = gethostbyname(hostname.c_str());
		if(!he)
			throw Exception("connect_socket","Unknown host");
		
		struct sockaddr_in serv_addr;
		serv_addr.sin_family = AF_INET;
		memcpy(&serv_addr.sin_addr, he->h_addr_list[0], he->h_length);
		serv_addr.sin_port = htons(port);
		
		if(connect(s, (struct sockaddr *)&serv_addr, sizeof(serv_addr))!=0)
			throw Exception("connect_socket","Unable to connect");
	}
	else if(connection_str.substr(0,7)=="unix://")
	{
		string path = connection_str.substr(7,string::npos);
		if(path[0]=='\0')
			throw Exception("connect_socket","Missing path, expected format : unix://<path>");
		
		int path_len = path.length();
		if(path_len>=sizeof(sockaddr_un::sun_path))
			throw Exception("connect_socket","Unix socket path is too long");
		
		s =  socket(AF_UNIX, SOCK_STREAM, 0);
		
		struct sockaddr_un serv_addr;
		serv_addr.sun_family = AF_UNIX;
		strcpy(serv_addr.sun_path,path.c_str());
		
		if(connect(s, (struct sockaddr *)&serv_addr, sizeof(serv_addr))!=0)
			throw Exception("connect_socket","Unable to connect");
	}
	else
		throw Exception("connect_socket","Invalid connection string format. Expected : tcp://<hostname>[:<port>] or unix://<path>");
	
	return s;
}