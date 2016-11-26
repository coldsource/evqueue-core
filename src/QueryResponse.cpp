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

#include <QueryResponse.h>
#include <XMLUtils.h>
#include <Logger.h>

#include <xqilla/xqilla-dom3.hpp>

#include <sys/types.h>
#include <sys/socket.h>

using namespace std;

QueryResponse::QueryResponse(int socket, const string &root_node_name)
{
	this->socket = socket;
	
	DOMImplementation *xqillaImplementation = DOMImplementationRegistry::getDOMImplementation(X("XPath2 3.0"));
	xmldoc = xqillaImplementation->createDocument();
	
	DOMElement *response_node = xmldoc->createElement(X(root_node_name.c_str()));
	xmldoc->appendChild(response_node);
	
	status_ok = true;
}

QueryResponse::~QueryResponse()
{
	xmldoc->release();
}

void QueryResponse::SetError(const string &error)
{
	this->error = error;
	status_ok = false;
}

void QueryResponse::SetAttribute(const std::string &name, const std::string &value)
{
	xmldoc->getDocumentElement()->setAttribute(X(name.c_str()),X(value.c_str()));
}

DOMNode *QueryResponse::AppendXML(const string &xml, DOMElement *node)
{
	if(!node)
		node = xmldoc->getDocumentElement();
	return XMLUtils::AppendXML(xmldoc, node, xml);
}

void QueryResponse::SendResponse()
{
	DOMImplementation *xqillaImplementation = DOMImplementationRegistry::getDOMImplementation(X("XPath2 3.0"));
	
	DOMElement *response_node = xmldoc->getDocumentElement();
	
	if(status_ok)
		response_node->setAttribute(X("status"),X("OK"));
	else
	{
		response_node->setAttribute(X("status"),X("KO"));
		response_node->setAttribute(X("error"),X(error.c_str()));
	}
	
	DOMLSSerializer *serializer = xqillaImplementation->createLSSerializer();
	XMLCh *response = serializer->writeToString(response_node);
	char *response_c = XMLString::transcode(response);
	
	send(socket,response_c,strlen(response_c),0);
	send(socket,"\n",1,0);
	
	XMLString::release(&response);
	XMLString::release(&response_c);
	serializer->release();
}

bool QueryResponse::Ping()
{
	Logger::Log(LOG_DEBUG, "Checking for dead peer");
	int re = send(socket,"<ping />\n",9,0);
	if(re!=9)
		return false;
	return true;
}
