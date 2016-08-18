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

#include <SocketResponseSAX2Handler.h>
#include <Exception.h>

#include <stdio.h>
#include <ctype.h>

#include <xqilla/xqilla-dom3.hpp>
#include <xercesc/sax2/Attributes.hpp>
#include <xercesc/dom/DOM.hpp>

using namespace std;
using namespace xercesc;

SocketResponseSAX2Handler::SocketResponseSAX2Handler(const string &context, bool record):SocketSAX2HandlerInterface(context)
{
	this->record = record;
	if(record)
	{
		DOMImplementation *xqillaImplementation = DOMImplementationRegistry::getDOMImplementation(X("XPath2 3.0"));
		xmldoc = xqillaImplementation->createDocument();
	}
	
	level = 0;
	ready = false;
}

SocketResponseSAX2Handler::~SocketResponseSAX2Handler()
{
	if(record)
		xmldoc->release();
}

void SocketResponseSAX2Handler::startElement(const XMLCh* const uri, const XMLCh* const localname, const XMLCh* const qname, const Attributes& attrs)
{
	char *node_name_c = XMLString::transcode(localname);
	
	level++;
	
	if(record)
	{
		// Store XML in DOM document, recreating all elements
		DOMElement *node = xmldoc->createElement(localname);
		
		for(int i=0;i<attrs.getLength();i++)
		{
			const XMLCh *attr_name, *attr_value;
			attr_name = attrs.getLocalName(i);
			attr_value = attrs.getValue(i);
			
			node->setAttribute(attr_name,attr_value);
		}
		
		if(level==1)
			xmldoc->appendChild(node);
		else
			current_node.at(level-2)->appendChild(node);
		
		current_node.push_back(node);
	}
	
	try
	{
		if(level==1)
		{
			group = node_name_c;
			
			for(int i=0;i<attrs.getLength();i++)
			{
				const XMLCh *attr_name, *attr_value;
				attr_name = attrs.getLocalName(i);
				attr_value = attrs.getValue(i);
				
				char *attr_name_c, *attr_value_c;
				attr_name_c = XMLString::transcode(attr_name);
				attr_value_c = XMLString::transcode(attr_value);
				
				root_attributes[attr_name_c] = attr_value_c;
				
				XMLString::release(&attr_name_c);
				XMLString::release(&attr_value_c);
			}
		}
	}
	catch(Exception e)
	{
		XMLString::release(&node_name_c);
		
		throw e;
	}
	
	XMLString::release(&node_name_c);
}

void SocketResponseSAX2Handler::endElement (const XMLCh *const uri, const XMLCh *const localname, const XMLCh *const qname)
{
	level--;
	
	if(record)
		current_node.pop_back();
	
	if (level==0) {
		ready = true;
		throw 0;  // get out of the parseNext loop
	}
}

void SocketResponseSAX2Handler::endDocument ()
{
	ready = true;
}
