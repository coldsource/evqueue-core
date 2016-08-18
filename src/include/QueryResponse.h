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

#include <xercesc/dom/DOM.hpp>

#include <string>

using namespace xercesc;

class QueryResponse
{
	int socket;
	
	DOMDocument *xmldoc;
	
	bool status_ok;
	std::string error;
	
	public:
		QueryResponse(int socket);
		~QueryResponse();
		
		DOMDocument *GetDOM() { return xmldoc; }
		void SetError(const std::string &error);
		
		DOMNode *AppendXML(const std::string &xml);
		
		void SendResponse();
		bool Ping();
};

#endif