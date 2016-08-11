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

#include <xqilla/xqilla-dom3.hpp>

#include <sys/types.h>
#include <sys/socket.h>

using namespace std;

QueryResponse::QueryResponse(int socket)
{
	this->socket = socket;
	
	DOMImplementation *xqillaImplementation = DOMImplementationRegistry::getDOMImplementation(X("XPath2 3.0"));
	xmldoc = xqillaImplementation->createDocument();
	
	DOMElement *response_node = xmldoc->createElement(X("response"));
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

DOMNode *QueryResponse::AppendXML(const string &xml)
{
	DOMImplementation *xqillaImplementation = DOMImplementationRegistry::getDOMImplementation(X("XPath2 3.0"));
	DOMLSParser *parser = xqillaImplementation->createLSParser(DOMImplementationLS::MODE_SYNCHRONOUS,0);
	
	DOMLSInput *input = xqillaImplementation->createLSInput();
	
	// Set XML content and parse document
	XMLCh *xml_xmlch = XMLString::transcode(xml.c_str());
	input->setStringData(xml_xmlch);
	DOMDocument *fragment_doc = parser->parse(input);
	
	XMLString::release(&xml_xmlch);
	input->release();
	
	DOMNode *node = xmldoc->importNode(fragment_doc->getDocumentElement(),true);
	xmldoc->getDocumentElement()->appendChild(node);
	
	parser->release();
	
	return node;
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
	
	XMLString::release(&response);
	XMLString::release(&response_c);
	serializer->release();
}

bool QueryResponse::Ping()
{
	int re = send(socket,"<ping/>",7,0);
	if(re!=7)
		return false;
	return true;
}