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

#include <AuthHandler.h>
#include <Logger.h>
#include <SocketSAX2Handler.h>
#include <SocketResponseSAX2Handler.h>
#include <Exception.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>

using namespace std;

AuthHandler::AuthHandler(int socket)
{
	this->socket = socket;
}

void AuthHandler::HandleAuth()
{
	send(socket,"<auth challenge='toto' />\n",27,0);
	
	try
	{
		SocketResponseSAX2Handler saxh("Authentication Handler");
		
		SocketSAX2Handler socket_sax2_handler(socket);
		socket_sax2_handler.HandleQuery(&saxh);
		
		const string response = saxh.GetRootAttribute("response");
		
		printf("%s\n",response.c_str());
	}
	catch(Exception &e)
	{
		throw e;
	}
}