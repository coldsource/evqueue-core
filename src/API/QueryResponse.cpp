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

#include <API/QueryResponse.h>
#include <XML/XMLUtils.h>
#include <Logger/Logger.h>
#include <DOM/DOMDocument.h>
#include <Configuration/ConfigurationEvQueue.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <memory>

using namespace std;

void QueryResponse::init(const string &root_node_name)
{
	xmldoc = new DOMDocument();
	xmldoc->appendChild(xmldoc->createElement(root_node_name)); // Append first root
	
	this->root_node_name = root_node_name;
	Empty();
	
	status_ok = true;
}

QueryResponse::QueryResponse(const string &root_node_name)
{
	init(root_node_name);
	
	this->socket = -1;
	this->wsi = 0;
}

QueryResponse::QueryResponse(int socket, const string &root_node_name)
{
	init(root_node_name);
	
	this->socket = socket;
	this->wsi = 0;
}

QueryResponse::QueryResponse(struct lws *wsi, const string &root_node_name)
{
	init(root_node_name);
	
	this->socket = -1;
	this->wsi = wsi;
}

QueryResponse::~QueryResponse()
{
	delete xmldoc;
}

void QueryResponse::SetError(const string &error)
{
	this->error = error;
	status_ok = false;
}

void QueryResponse::SetErrorCode(const string &code)
{
	this->error_code = code;
	status_ok = false;
}

void QueryResponse::SetAttribute(const std::string &name, const std::string &value)
{
	xmldoc->getDocumentElement().setAttribute(name,value);
}

DOMNode QueryResponse::AppendXML(const string &xml)
{
	return XMLUtils::AppendXML(xmldoc, xmldoc->getDocumentElement(), xml);
}

DOMNode QueryResponse::AppendText(const string &text)
{
	return XMLUtils::AppendText(xmldoc, xmldoc->getDocumentElement(), text);
}

DOMNode QueryResponse::AppendXML(const string &xml, DOMElement node)
{
	return XMLUtils::AppendXML(xmldoc, node, xml);
}

void QueryResponse::Empty()
{
	xmldoc->removeChild(xmldoc->getDocumentElement());
	
	DOMElement response_node = xmldoc->createElement(root_node_name);
	xmldoc->appendChild(response_node);
	
	response_node.setAttribute("node",ConfigurationEvQueue::GetInstance()->Get("cluster.node.name"));
}

void QueryResponse::SendResponse()
{
	DOMElement response_node = xmldoc->getDocumentElement();
	
	if(status_ok)
		response_node.setAttribute("status","OK");
	else
	{
		response_node.setAttribute("status","KO");
		
		if(error!="")
			response_node.setAttribute("error",error);
		
		if(error_code!="")
			response_node.setAttribute("error-code",error_code);
	}
	
	string response = xmldoc->Serialize(xmldoc->getDocumentElement());
	
	if(socket!=-1)
	{
		send(socket,response.c_str(),response.length(),0);
		send(socket,"\n",1,0);
	}
	
	if(wsi)
	{
		response.insert(0,LWS_PRE,' ');
		lws_write(wsi, (unsigned char *)response.c_str() + LWS_PRE, response.length()-LWS_PRE, LWS_WRITE_TEXT);
	}
}

bool QueryResponse::Ping()
{
	Logger::Log(LOG_DEBUG, "Checking for dead peer");
	int re = send(socket,"<ping />\n",9,0);
	if(re!=9)
		return false;
	return true;
}
