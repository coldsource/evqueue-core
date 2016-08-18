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
#include <Exception.h>
#include <SocketResponseSAX2Handler.h>
#include <hmac.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <netdb.h>

#include <string.h>

#include <xqilla/xqilla-dom3.hpp>

using namespace std;
using namespace xercesc;

ClientBase::ClientBase(const string &connection_str, const string &user, const string &password)
{
	connected = false;
	authenticated = false;
	
	this->connection_str = connection_str;
	this->user = user;
	this->password = password;
	
	// Parse connection string
	if(connection_str.substr(0,6)=="tcp://")
	{
		connection_type = tcp;
		
		if(connection_str.length()==6)
			throw Exception("Client","Missing hostname, expected format : tcp://<hostname>[:<port>]");
		
		host = connection_str.substr(6,string::npos);
		
		size_t port_pos = host.find(":");
		if(port_pos==string::npos)
			port = 5000;
		else
		{
			string port_str = host.substr(port_pos+1,string::npos);
			host = host.substr(0,port_pos);
			
			if(port_str.length()==0)
				throw Exception("Client","Missing hostname, expected format : tcp://<hostname>[:<port>]");
			
			try
			{
				port = std::stoi(port_str	);
			}
			catch(...)
			{
				throw Exception("Client","Invalid port number");
			}
		}
		
		s =  socket(AF_INET, SOCK_STREAM, 0);
		if(s==-1)
			throw Exception("Client","Unable to create TCP socket");
	}
	else if(connection_str.substr(0,7)=="unix://")
	{
		connection_type = unix;
		
		host = connection_str.substr(7,string::npos);
		if(host[0]=='\0')
			throw Exception("Client","Missing path, expected format : unix://<path>");
		
		int path_len = host.length();
		if(path_len>=sizeof(sockaddr_un::sun_path))
			throw Exception("Client","Unix socket path is too long");
		
		s =  socket(AF_UNIX, SOCK_STREAM, 0);
		if(s==-1)
			throw Exception("Client","Unable to create UNIX socket");
	}
	else
		throw Exception("Client","Invalid connection string format. Expected : tcp://<hostname>[:<port>] or unix://<path>");
}

ClientBase::~ClientBase()
{
	if(s!=-1)
		close(s);
	
	if(saxh)
		delete saxh;
}

void ClientBase::Exec(const std::string &cmd, bool record)
{
	if(!connected)
		connect();
	
	send(cmd);
	recv(record);
	
	if(saxh->GetGroup()!="response")
		throw Exception("Client","Invalid response from server");
	
	if(saxh->GetRootAttribute("status")!="OK")
	{
		string error = saxh->GetRootAttribute("error");
		
		throw Exception("Client","Error executing command : "+error);
	}
}

DOMDocument *ClientBase::GetResponseDOM()
{
	if(!saxh)
		return 0;
	
	return saxh->GetDOM();
}

void ClientBase::connect()
{
	// Nothing to do
	if(connected)
		return;
	
	// Connect to the server
	if(connection_type==tcp)
	{
		struct hostent *he;
		he = gethostbyname(host.c_str());
		if(!he)
			throw Exception("Client","Unknown host");
		
		struct sockaddr_in serv_addr;
		serv_addr.sin_family = AF_INET;
		memcpy(&serv_addr.sin_addr, he->h_addr_list[0], he->h_length);
		serv_addr.sin_port = htons(port);
		
		if(::connect(s, (struct sockaddr *)&serv_addr, sizeof(serv_addr))!=0)
			throw Exception("Client","Unable to connect");
	}
	else if(connection_type==unix)
	{
		struct sockaddr_un serv_addr;
		serv_addr.sun_family = AF_UNIX;
		strcpy(serv_addr.sun_path,host.c_str());
		
		if(::connect(s, (struct sockaddr *)&serv_addr, sizeof(serv_addr))!=0)
			throw Exception("Client","Unable to connect");
	}
	
	connected = true;
	
	// Read server greeting
	recv();
	
	if(saxh->GetGroup()=="ready")
		authenticated = true;
	else
		authenticate();
}

void ClientBase::send(const std::string &cmd)
{
	if(::send(s,cmd.c_str(),cmd.length(),0)!=cmd.length())
		throw Exception("Client","Error sending data to server");
}

void ClientBase::recv(bool record)
{
	// Remplace old handler
	if(saxh)
		delete saxh;
	
	saxh = new SocketResponseSAX2Handler("Client",record);
	
	SocketSAX2Handler socket_sax2_handler(s);
	socket_sax2_handler.HandleQuery(saxh);
}

void ClientBase::authenticate()
{
	if(authenticated)
		return;
	
	if(user.length()==0 || password.length()==0)
			throw Exception("Client","Authentication is required but not user/password has been provided");
	
	string response = hash_hmac(password, saxh->GetRootAttribute("challenge"));
	
	send("<auth response='"+response+"' user='"+user+"' />");
	recv();
	
	if(saxh->GetGroup()=="ready")
		throw Exception("Client","Authentication error");
	
	authenticated = true;
}