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

#include <SocketQuerySAX2Handler.h>
#include <Exception.h>
#include <XMLString.h>
#include <DOMNamedNodeMap.h>
#include <DOMDocument.h>

#include <stdio.h>
#include <ctype.h>

#include <xercesc/sax2/Attributes.hpp>

#include <memory>

using namespace std;

SocketQuerySAX2Handler::SocketQuerySAX2Handler(const string &context):SocketSAX2HandlerInterface(context)
{
	level = 0;
	ready = false;
}

SocketQuerySAX2Handler::SocketQuerySAX2Handler(const string &context, const string xml):SocketSAX2HandlerInterface(context)
{
	unique_ptr<DOMDocument> xmldoc(DOMDocument::Parse(xml));
	if(!xmldoc)
		throw Exception("API Response","Invalid XML document");
	
	group = xmldoc->getDocumentElement().getNodeName();
	
	DOMNamedNodeMap attributes = xmldoc->getDocumentElement().getAttributes();
	for(int i=0;i<attributes.getLength();i++)
	{
		DOMNode attribute = attributes.item(i);
		string name = attribute.getNodeName();
		string value = attribute.getNodeValue();
		root_attributes[name] = value;
	}
	
	ready = true;
}

SocketQuerySAX2Handler::~SocketQuerySAX2Handler()
{
}

void SocketQuerySAX2Handler::startElement(const XMLCh* const uri, const XMLCh* const localname, const XMLCh* const qname, const xercesc::Attributes& attrs)
{
	string node_name = XMLString(localname);
	string name;
	string value;
	
	level++;
	
	if(level==1)
	{
		// Generic data storage for Query Handlers
		group = node_name;
		
		for(int i=0;i<attrs.getLength();i++)
		{
			string attr_name = XMLString(attrs.getLocalName(i));
			string attr_value = XMLString(attrs.getValue(i));
			
			root_attributes[attr_name] = attr_value;
		}
	}
		
	if((group=="instance" || group=="instances" || group=="workflow_schedule") && level==2)
	{
		if(node_name!="parameter")
			throw Exception("SocketQuerySAX2Handler","Expecting parameter node");
		
		const XMLCh *name_xml = attrs.getValue(XMLString("name"));
		const XMLCh *value_xml = attrs.getValue(XMLString("value"));
		
		if (name_xml==0 || value_xml==0)
			throw Exception("SocketQuerySAX2Handler","Invalue parameter node, missing name or value attributes");
		
		name = XMLString(name_xml);
		value = XMLString(value_xml);
		
		int i;
		int name_len = name.length();
		
		if(name_len>PARAMETER_NAME_MAX_LEN)
			throw Exception("SocketQuerySAX2Handler","Parameter name is too long");
		
		for(i=0;i<name_len;i++)
			if(!isalnum(name[i]) && name[i]!='_')
				throw Exception("SocketQuerySAX2Handler","Invalid parameter name");
		
		if(!params.Add(name,value))
			throw Exception("SocketQuerySAX2Handler","Duplicate parameter name");
	}
	
	if(group=="task" && level==2)
	{
		if(node_name!="input")
			throw Exception("SocketQuerySAX2Handler","Expecting input node");
		
		value = XMLString(attrs.getValue(XMLString("value")));
		
		inputs.push_back(value);
	}
	
	if(group=="task" && level>2)
		throw Exception("SocketQuerySAX2Handler","input node does not accept subnodes");
	
	if(group!="instance" && group!="instances" && group!="workflow_schedule" && group!="task" && level>1)
		throw Exception("SocketQuerySAX2Handler","Unexpected subnode");
}

void SocketQuerySAX2Handler::endElement (const XMLCh *const uri, const XMLCh *const localname, const XMLCh *const qname)
{
	level--;
	if (level==0) {
		ready = true;
		throw 0;  // get out of the parseNext loop
	}
}

void SocketQuerySAX2Handler::endDocument ()
{
	ready = true;
}
