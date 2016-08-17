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

#include <stdio.h>
#include <ctype.h>

#include <xqilla/xqilla-dom3.hpp>
#include <xercesc/sax2/Attributes.hpp>

using namespace std;

SocketQuerySAX2Handler::SocketQuerySAX2Handler(const string &context):SocketSAX2HandlerInterface(context)
{
	level = 0;
	ready = false;
}

SocketQuerySAX2Handler::~SocketQuerySAX2Handler()
{
}


void SocketQuerySAX2Handler::startElement(const XMLCh* const uri, const XMLCh* const localname, const XMLCh* const qname, const Attributes& attrs)
{
	char *node_name_c = XMLString::transcode(localname);
	char *name_c = 0;
	char *value_c = 0;
	
	level++;
	
	try
	{
		if(level==1)
		{
			// Generic data storage for Query Handlers
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
			
		if((group=="instance" || group=="instances" || group=="workflow_schedule") && level==2)
		{
			if(strcmp(node_name_c,"parameter")!=0)
				throw Exception("SocketQuerySAX2Handler","Expecting parameter node");
			
			const XMLCh *name = attrs.getValue(X("name"));
			const XMLCh *value = attrs.getValue(X("value"));
			
			if (name==0 || value==0)
				throw Exception("SocketQuerySAX2Handler","Invalue parameter node, missing name or value attributes");
			
			name_c = XMLString::transcode(name);
			value_c = XMLString::transcode(value);
			
			int i;
			int name_len = strlen(name_c);
			
			if(name_len>PARAMETER_NAME_MAX_LEN)
				throw Exception("SocketQuerySAX2Handler","Parameter name is too long");
			
			for(i=0;i<name_len;i++)
				if(!isalnum(name[i]) && name[i]!='_')
					throw Exception("SocketQuerySAX2Handler","Invalid parameter name");
			
			if(!params.Add(name_c,value_c))
				throw Exception("SocketQuerySAX2Handler","Duplicate parameter name");
		}
		
		if(group=="task" && level==2)
		{
			if(strcmp(node_name_c,"input")!=0)
				throw Exception("SocketQuerySAX2Handler","Expecting input node");
			
			const XMLCh *value = attrs.getValue(X("value"));
			value_c = XMLString::transcode(value);
			
			inputs.push_back(value_c);
		}
		
		if(group=="task" && level>2)
			throw Exception("SocketQuerySAX2Handler","input node does not accept subnodes");
		
		if(group!="instance" && group!="instances" && group!="workflow_schedule" && group!="task" && level>1)
			throw Exception("SocketQuerySAX2Handler","Unexpected subnode");
	}
	catch(Exception e)
	{
		if(name_c)
			XMLString::release(&name_c);
		if(value_c)
			XMLString::release(&value_c);
		XMLString::release(&node_name_c);
		
		throw e;
	}
	
	if(name_c)
		XMLString::release(&name_c);
	if(value_c)
		XMLString::release(&value_c);
	XMLString::release(&node_name_c);
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
