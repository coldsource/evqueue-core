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

#ifndef _NETWORKCONNECTIONS_H_
#define _NETWORKCONNECTIONS_H_

#include <string>
#include <vector>

class NetworkConnections
{
	public:
		typedef void (*t_stream_handler) (int s);
		typedef void (*t_dgram_handler) (char *str, size_t len);
	
	private:
		enum en_type
		{
			TCP,
			UNIX,
			UDP
		};
		
		struct st_conn
		{
			int socket;
			std::string name;
			en_type type;
			int maxlen;
			t_stream_handler stream_handler;
			t_dgram_handler dgram_handler;
		};
		
		std::vector<st_conn> connections;
		
		static NetworkConnections *instance;
	
	public:
		NetworkConnections();
		
		static NetworkConnections *GetInstance() { return instance; }
		
		void RegisterTCP(const std::string &name, const std::string &bind_ip, int port, int backlog, t_stream_handler cbk);
		void RegisterUNIX(const std::string &name, const std::string &bind_path, int backlog, t_stream_handler cbk);
		void RegisterUDP(const std::string &name, const std::string &bind_ip, int port, int maxlen, t_dgram_handler cbk);
		
		bool select();
};

#endif
