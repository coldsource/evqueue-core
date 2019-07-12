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
#include <XMLString.h>

#include <stdio.h>
#include <ctype.h>

#include <xercesc/sax2/Attributes.hpp>
#include <xercesc/dom/DOM.hpp>

using namespace std;

SocketResponseSAX2Handler::SocketResponseSAX2Handler(const string &context, bool record):SocketSAX2HandlerInterface(context)
{
	this->record = record;

	level = 0;
	ready = false;
}

SocketResponseSAX2Handler::~SocketResponseSAX2Handler()
{
}

void SocketResponseSAX2Handler::startElement(const XMLCh* const uri, const XMLCh* const localname, const XMLCh* const qname, const xercesc::Attributes& attrs)
{
	XMLString node_name(localname);
	
	level++;
	
	if(record)
	{
		// Store XML in DOM document, recreating all elements
		DOMElement node = xmldoc.createElement(node_name);
		
		for(int i=0;i<attrs.getLength();i++)
		{
			const XMLCh *attr_name, *attr_value;
			attr_name = attrs.getLocalName(i);
			attr_value = attrs.getValue(i);
			
			node.setAttribute(XMLString(attr_name),XMLString(attr_value));
		}
		
		if(level==1)
			xmldoc.appendChild(node);
		else
			current_node.at(level-2).appendChild(node);
		
		current_node.push_back(node);
	}
	
	if(level==1)
	{
		group = node_name;
		
		for(int i=0;i<attrs.getLength();i++)
		{
			const XMLCh *attr_name, *attr_value;
			attr_name = attrs.getLocalName(i);
			attr_value = attrs.getValue(i);
			
			root_attributes[XMLString(attr_name)] = XMLString(attr_value);
		}
	}
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

void SocketResponseSAX2Handler::characters(const XMLCh *const chars, const XMLSize_t length)
{
	if(record)
	{
		XMLCh *chars_nt = new XMLCh[length+1];
		memcpy(chars_nt,chars,length*sizeof(XMLCh));
		chars_nt[length] = 0;
		
		current_text_node = xmldoc.createTextNode(XMLString(chars_nt));
		current_node.at(level-1).appendChild(current_text_node);
		
		delete[] chars_nt;
	}
}

void SocketResponseSAX2Handler::endDocument ()
{
	ready = true;
}
