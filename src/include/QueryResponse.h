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

#ifndef _QUERYRESPONSE_H_
#define _QUERYRESPONSE_H_

#include <DOMElement.h>
#include <DOMDocument.h>

#include <string>

#include <libwebsockets.h>

class QueryResponse
{
	int socket;
	struct lws *wsi;
	
	std::string root_node_name;
	
	DOMDocument *xmldoc;
	
	bool status_ok;
	std::string error;
	std::string error_code;
	 
	 void init(const std::string &root_node_name);
	
	public:
		QueryResponse(const std::string &root_node_name = "response");
		QueryResponse(int socket, const std::string &root_node_name = "response");
		QueryResponse(struct lws *wsi, const std::string &root_node_name = "response");
		~QueryResponse();
		
		void SetSocket(int s) { this->socket = s; }
		void SetWebsocket(struct lws *wsi) { this->wsi = wsi; }
		
		DOMDocument *GetDOM() { return xmldoc; }
		void SetError(const std::string &error);
		void SetErrorCode(const std::string &code);
		void SetAttribute(const std::string &name, const std::string &value);
		
		DOMNode AppendXML(const std::string &xml);
		DOMNode AppendText(const std::string &text);
		DOMNode AppendXML(const std::string &xml, DOMElement node);
		
		void Empty();
		
		void SendResponse();
		bool Ping();
};

#endif
