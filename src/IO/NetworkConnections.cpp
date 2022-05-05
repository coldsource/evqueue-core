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

#include <IO/NetworkConnections.h>
#include <Logger/Logger.h>
#include <Exception/Exception.h>
#include <global.h>

#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <string.h>
#include <errno.h>
 
using namespace std;

NetworkConnections *NetworkConnections::instance = 0;

NetworkConnections::NetworkConnections()
{
	instance = this;
}

void NetworkConnections::RegisterTCP(const string &name, const string &bind_ip, int port, int backlog, t_stream_handler cbk)
{
	int listen_socket, re, optval;
	
	// Create listen socket
	listen_socket = socket(PF_INET,SOCK_STREAM | SOCK_CLOEXEC,0);
	
	// Configure socket
	optval=1;
	setsockopt(listen_socket,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(int));
	
	// Bind socket
	struct sockaddr_in local_addr;
	memset(&local_addr,0,sizeof(struct sockaddr_in));
	local_addr.sin_family=AF_INET;
	if(bind_ip=="*")
		local_addr.sin_addr.s_addr=htonl(INADDR_ANY);
	else
		local_addr.sin_addr.s_addr=inet_addr(bind_ip.c_str());
	local_addr.sin_port = htons(port);
	re = bind(listen_socket,(struct sockaddr *)&local_addr,sizeof(struct sockaddr_in));
	if(re==-1)
		throw Exception("core","Unable to bind "+name+" listen socket");
	
	// Listen on socket
	re = listen(listen_socket,backlog);
	if(re==-1)
		throw Exception("core","Unable to listen on "+name+" socket");
	
	Logger::Log(LOG_INFO,"%s listen backlog set to %d (tcp socket)",name.c_str(), backlog);
	
	Logger::Log(LOG_NOTICE,"%s accepting connections on port %d", name.c_str(), port);
	
	connections.push_back({listen_socket, name, TCP, 0, cbk, 0});
}

void NetworkConnections::RegisterUNIX(const string &name, const string &bind_path, int backlog, t_stream_handler cbk)
{
	int listen_socket_unix, re;
	
	// Create UNIX socket
	listen_socket_unix = socket(AF_UNIX,SOCK_STREAM | SOCK_CLOEXEC,0);
	
	// Bind socket
	struct sockaddr_un local_addr_unix;
	local_addr_unix.sun_family=AF_UNIX;
	strcpy(local_addr_unix.sun_path,bind_path.c_str());
	unlink(local_addr_unix.sun_path);
	re = bind(listen_socket_unix,(struct sockaddr *)&local_addr_unix,sizeof(struct sockaddr_un));
	if(re==-1)
		throw Exception("core","Unable to bind "+name+" unix listen socket");
	
	// Listen on socket
	chmod(bind_path.c_str(),0777);
	re = listen(listen_socket_unix,backlog);
	if(re==-1)
		throw Exception("core","Unable to listen on "+name+" unix socket");
	
	Logger::Log(LOG_INFO,"%s listen backlog set to %d (unix socket)",name.c_str(), backlog);
	
	Logger::Log(LOG_NOTICE,"%s accepting connections on unix socket %s", name.c_str(), bind_path.c_str());
	
	connections.push_back({listen_socket_unix, name, UNIX, 0, cbk, 0});
}

void NetworkConnections::RegisterUDP(const string &name, const string &bind_ip, int port, int maxlen, t_dgram_handler cbk)
{
	int listen_socket_udp, re, optval;
	
	// Create listen socket
	listen_socket_udp = socket(PF_INET,SOCK_DGRAM | SOCK_CLOEXEC,0);
	
	// Configure socket
	optval=1;
	setsockopt(listen_socket_udp,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(int));
	
	// Bind socket
	struct sockaddr_in local_addr;
	memset(&local_addr,0,sizeof(struct sockaddr_in));
	local_addr.sin_family=AF_INET;
	if(bind_ip=="*")
		local_addr.sin_addr.s_addr=htonl(INADDR_ANY);
	else
		local_addr.sin_addr.s_addr=inet_addr(bind_ip.c_str());
	local_addr.sin_port = htons(port);
	re = bind(listen_socket_udp,(struct sockaddr *)&local_addr,sizeof(struct sockaddr_in));
	if(re==-1)
		throw Exception("core","Unable to bind "+name+" listen socket");
	
	Logger::Log(LOG_NOTICE,"%s waiting UDP messages on port %d",name.c_str(), port);
	
	connections.push_back({listen_socket_udp, name, UDP, maxlen, 0, cbk});
}

bool NetworkConnections::select()
{
	fd_set rfds;
	
	FD_ZERO(&rfds);
	int fdmax = -1;
	
	for(int i=0;i<connections.size();i++)
	{
		FD_SET(connections[i].socket,&rfds);
		fdmax = MAX(fdmax, connections[i].socket);
	}
	
	int re = ::select(fdmax+1,&rfds,0,0,0);
	if(re<0)
	{
		if(errno==EINTR)
			return true; // Interrupted by signal, continue
		
		return false;
	}
	
	for(int i=0;i<connections.size();i++)
	{
		if(!FD_ISSET(connections[i].socket,&rfds))
			continue;
		
		if(connections[i].type==TCP || connections[i].type==UNIX)
		{
			int s = accept4(connections[i].socket, 0, 0, SOCK_CLOEXEC);
			if(s<0)
				return true;
			
			connections[i].stream_handler(s);
		}
		else if(connections[i].type==UDP)
		{
			char buf[connections[i].maxlen];
			size_t len = recvfrom(connections[i].socket, buf, connections[i].maxlen, 0, 0, 0);
			connections[i].dgram_handler(buf, len);
		}
		
		return true;
	}
	
	return true;
}

void NetworkConnections::Shutdown()
{
	Logger::Log(LOG_NOTICE,"Shutdown requested, closing listening sockets");
	
	// Shutdown requested, close listening sockets
	for(int i=0;i<connections.size();i++)
		close(connections[i].socket);
}
