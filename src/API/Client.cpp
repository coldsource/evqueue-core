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

#include <API/Client.h>
#include <Exception/Exception.h>
#include <IO/Sockets.h>
#include <Logger/Logger.h>
#include <Crypto/hmac.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <netdb.h>

#include <string.h>

using namespace std;

Client::Client(const string &connection_str, const string &user, const string &password):ClientBase(connection_str,user,password)
{
	Sockets::GetInstance()->RegisterSocket(s);
}

Client::~Client()
{
	if(s!=-1)
	{
		Sockets::GetInstance()->UnregisterSocket(s);
		s = -1;
	}
}

void Client::Exec(const std::string &cmd)
{
	try
	{
		ClientBase::Exec(cmd);
	}
	catch(Exception &e)
	{
		Logger::Log(LOG_WARNING,"Client encountered unexpected exception in context "+e.context+" : "+e.error);
		throw e;
	}
}
