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

#ifndef _CLIENTBASE_H_
#define _CLIENTBASE_H_

#include <string>

#include <xercesc/dom/DOM.hpp>
using namespace xercesc;

class SocketResponseSAX2Handler;

class ClientBase
{
	bool connected;
	bool authenticated;
	
	std::string connection_str;
	std::string user;
	std::string password;
	
	enum t_connection_type{tcp,unix};
	t_connection_type connection_type;
	std::string host;
	int port;
	
	int cnx_timeout = 0;
	int snd_timeout = 0;
	int rcv_timeout = 0;

	protected:
		int s = -1;
	
		SocketResponseSAX2Handler *saxh = 0;
	
	public:
		ClientBase(const std::string &connection_str, const std::string &user, const std::string &password);
		virtual ~ClientBase();
		
		void Exec(const std::string &cmd, bool record = false);
		DOMDocument *GetResponseDOM();
		SocketResponseSAX2Handler *GetResponseHandler() { return saxh; }
		
		void SetTimeouts(int cnx_timeout,int snd_timeout,int rcv_timeout);
	
	protected:
		void connect();
		void send(const std::string &cmd);
		void recv(bool record = false);
		void authenticate();
		void disconnect();
};

#endif