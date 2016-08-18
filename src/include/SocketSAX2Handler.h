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

#ifndef _SOCKETSAX2HANDLER_H_
#define _SOCKETSAX2HANDLER_H_

#include <xercesc/sax2/DefaultHandler.hpp>

#include <map>
#include <string>

class SocketSAX2HandlerInterface : public xercesc::DefaultHandler
{
	protected:
		std::string context;
		
		std::map<std::string,std::string> root_attributes;
	
	public:
		SocketSAX2HandlerInterface(const std::string &context);
		
		virtual bool IsReady() = 0;
		
		const std::map<std::string,std::string> &GetRootAttributes() { return root_attributes; }
		
		const std::string &GetRootAttribute(const std::string &name);
		const std::string &GetRootAttribute(const std::string &name, const std::string &default_value);
		int GetRootAttributeInt(const std::string &name);
		int GetRootAttributeInt(const std::string &name, int default_value);
		bool GetRootAttributeBool(const std::string &name);
		bool GetRootAttributeBool(const std::string &name, bool default_value);
};

class SocketSAX2Handler
{
	int socket;
	
	public:
		SocketSAX2Handler(int socket);
		
		void HandleQuery(SocketSAX2HandlerInterface *handler);
};

#endif