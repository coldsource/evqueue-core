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
#include <Exception/Exception.h>
#include <API/XMLResponse.h>
#include <Crypto/hmac.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <netdb.h>
#include <fcntl.h>

#include <string.h>
#include <errno.h>
#include <unistd.h>

using namespace std;

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
	
	if(response)
		delete response;
}

const string& ClientBase::Connect(void)
{
	connect();
	
	return GetNode();
}

void ClientBase::Disconnect(void)
{
	disconnect();
}

void ClientBase::Exec(const std::string &cmd)
{
	if(!connected)
		connect();
	
	send(cmd);
	
	do
	{
		recv();
	} while(response->GetGroup()=="ping"); // Skip DPD pings from engine
	
	if(response->GetGroup()!="response")
		throw Exception("Client","Invalid response from server");
	
	if(response->GetRootAttribute("status")!="OK")
	{
		string error = response->GetRootAttribute("error");
		string error_code = response->GetRootAttribute("error-code","");
		
		string message = "Error executing command : "+error;
		if(error_code!="")
			message += " ("+error_code+")";
		
		throw Exception("Client",message);
	}
	
	disconnect();
}

DOMDocument *ClientBase::GetResponseDOM()
{
	if(!response)
		return 0;
	
	return response->GetDOM();
}

void ClientBase::SetTimeouts(int cnx_timeout,int snd_timeout,int rcv_timeout)
{
	this->cnx_timeout = cnx_timeout;
	this->snd_timeout = snd_timeout;
	this->rcv_timeout = rcv_timeout;
}

void ClientBase::connect()
{
	// Nothing to do
	if(connected)
		return;
	
	// Enable non-blocking IO to manage connect timeout
	if(cnx_timeout)
	{
		int flags = fcntl(s, F_GETFL, 0);
		fcntl(s, F_SETFL, flags | O_NONBLOCK);
	}
	
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
		
		if(::connect(s, (struct sockaddr *)&serv_addr, sizeof(serv_addr))!=0 && errno!=EINPROGRESS)
			throw Exception("Client","Unable to connect");
	}
	else if(connection_type==unix)
	{
		struct sockaddr_un serv_addr;
		serv_addr.sun_family = AF_UNIX;
		strcpy(serv_addr.sun_path,host.c_str());
		
		if(::connect(s, (struct sockaddr *)&serv_addr, sizeof(serv_addr))!=0 && errno!=EINPROGRESS)
			throw Exception("Client","Unable to connect");
	}
	
	if(cnx_timeout)
	{
		// Handle timeout
		struct timeval tv;
		tv.tv_sec = cnx_timeout;
		tv.tv_usec = 0;
		
		fd_set wfds;
		FD_ZERO(&wfds);
		FD_SET(s,&wfds);
		if(select(s+1,0,&wfds,0,&tv)!=1)
			throw Exception("Client","Unable to connect");
		
		// Enable blocking mode again
		int flags = fcntl(s, F_GETFL, 0);
		fcntl(s, F_SETFL, flags & (~O_NONBLOCK));
	}
	
	// Set socket timeouts if requested
	struct timeval tv;
	if(rcv_timeout)
	{
		tv.tv_sec = rcv_timeout;
		tv.tv_usec = 0;
		setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv,sizeof(struct timeval));
	}
	
	if(snd_timeout)
	{
		tv.tv_sec = snd_timeout;
		tv.tv_usec = 0;
		setsockopt(s, SOL_SOCKET, SO_SNDTIMEO,(struct timeval *)&tv,sizeof(struct timeval));
	}
	
	connected = true;
	
	// Read server greeting
	recv();
	
	if(response->GetGroup()=="ready")
	{
		authenticated = true;
		node = response->GetRootAttribute("node");
	}
	else
		authenticate();
}

void ClientBase::send(const std::string &cmd)
{
	if(::send(s,cmd.c_str(),cmd.length(),0)!=cmd.length())
		throw Exception("Client","Error sending data to server");
}

void ClientBase::recv()
{
	// Remplace old handler
	if(response)
		delete response;
	
	response = new XMLResponse("Client",s);
}

void ClientBase::authenticate()
{
	if(authenticated)
		return;
	
	if(user.length()==0 || password.length()==0)
		throw Exception("Client","Authentication is required but no user/password has been provided");
	
	string challenge_response = hash_hmac(password, response->GetRootAttribute("challenge"));
	
	send("<auth response='"+challenge_response+"' user='"+user+"' />");
	recv();
	
	if(response->GetGroup()!="ready")
		throw Exception("Client","Authentication error");
	
	authenticated = true;
	node = response->GetRootAttribute("node");
}

void ClientBase::disconnect()
{
	if(!connected)
		return;
	
	send("<quit />\n");
	
	connected = false;
}
