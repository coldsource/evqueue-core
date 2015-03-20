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

SocketQuerySAX2Handler::SocketQuerySAX2Handler()
{
	workflow_name = 0;
	workflow_host = 0;
	workflow_user = 0;
	level = 0;
	ready = false;
}

SocketQuerySAX2Handler::~SocketQuerySAX2Handler()
{
	if(workflow_name)
		XMLString::release(&workflow_name);
	
	if(workflow_host)
		XMLString::release(&workflow_host);
	
	if(workflow_user)
		XMLString::release(&workflow_user);
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
			if(strcmp(node_name_c,"statistics")==0)
			{
				const XMLCh *type_attr = attrs.getValue(X("type"));
				
				if(type_attr==0)
					throw Exception("SocketQuerySAX2Handler","Missing type attribute on node statistics");
				
				if(XMLString::compareString(type_attr,X("global"))==0)
				{
					const XMLCh *action_attr = attrs.getValue(X("action"));
					
					if(action_attr==0 || XMLString::compareString(action_attr,X("query"))==0)
						query_type = SocketQuerySAX2Handler::QUERY_GLOBAL_STATS;
					else if(XMLString::compareString(action_attr,X("reset"))==0)
						query_type = SocketQuerySAX2Handler::RESET_GLOBAL_STATS;
					else
						throw Exception("SocketQuerySAX2Handler","Unknown statistics action");
				}
				else if(XMLString::compareString(type_attr,X("queue"))==0)
					query_type = SocketQuerySAX2Handler::QUERY_QUEUE_STATS;
				else
					throw Exception("SocketQuerySAX2Handler","Unknown statistics type");
			}
			else if(strcmp(node_name_c,"status")==0)
			{
				const XMLCh *type_attr = attrs.getValue(X("type"));
				
				if(type_attr==0)
					throw Exception("SocketQuerySAX2Handler","Missing type attribute on node statistics");
				
				if(XMLString::compareString(type_attr,X("scheduler"))==0)
					query_type = SocketQuerySAX2Handler::QUERY_SCHEDULER_STATUS;
				else if(XMLString::compareString(type_attr,X("workflows"))==0)
					query_type = SocketQuerySAX2Handler::QUERY_WORKFLOWS_STATUS;
				else
					throw Exception("SocketQuerySAX2Handler","Unknown statistics type");
			}
			else if(strcmp(node_name_c,"workflow")==0)
			{
				const XMLCh *name_attr = attrs.getValue(X("name"));
				const XMLCh *id_attr = attrs.getValue(X("id"));
				
				if (name_attr==0 && id_attr==0)
					throw Exception("SocketQuerySAX2Handler","Missing name or id attribute on node workflow");
					
				if(name_attr!=0)
				{
					query_type = SocketQuerySAX2Handler::QUERY_WORKFLOW_LAUNCH;
					workflow_name = XMLString::transcode(name_attr);
					
					int len = strlen(workflow_name);
					for(int i=0;i<len;i++)
						if(!isalnum(workflow_name[i]) && workflow_name[i]!='_')
							throw Exception("SocketQuerySAX2Handler","Invalid workflow name");
					
					const XMLCh *mode_attr = attrs.getValue(X("mode"));
					if(mode_attr==0 || XMLString::compareString(mode_attr,X("asynchronous"))==0)
						query_options = SocketQuerySAX2Handler::QUERY_OPTION_MODE_ASYNCHRONOUS;
					else if(XMLString::compareString(mode_attr,X("synchronous"))==0)
						query_options = SocketQuerySAX2Handler::QUERY_OPTION_MODE_SYNCHRONOUS;
					
					const XMLCh *host_attr = attrs.getValue(X("host"));
					if(host_attr!=0)
						workflow_host = XMLString::transcode(host_attr);
					
					const XMLCh *user_attr = attrs.getValue(X("user"));
					if(user_attr!=0)
						workflow_user = XMLString::transcode(user_attr);
					
				}
				else
				{
					const XMLCh *action_attr = attrs.getValue(X("action"));
					workflow_id = XMLString::parseInt(id_attr);
					
					if(action_attr==0 || XMLString::compareString(action_attr,X("info"))==0)
						query_type = SocketQuerySAX2Handler::QUERY_WORKFLOW_INFO;
					else if(XMLString::compareString(action_attr,X("cancel"))==0)
						query_type = SocketQuerySAX2Handler::QUERY_WORKFLOW_CANCEL;
					else if(XMLString::compareString(action_attr,X("wait"))==0)
						query_type = SocketQuerySAX2Handler::QUERY_WORKFLOW_WAIT;
					else if(XMLString::compareString(action_attr,X("killtask"))==0)
					{
						query_type = SocketQuerySAX2Handler::QUERY_WORKFLOW_KILLTASK;
						
						const XMLCh *pid_attr = attrs.getValue(X("pid"));
						if(pid_attr==0)
							throw Exception("SocketQuerySAX2Handler","Action killtask requires a PID");
						
						task_pid = XMLString::parseInt(pid_attr);
					}
				}
			}
			else
				throw Exception("SocketQuerySAX2Handler","Invalid root node name, expecting 'workflow' or 'statistics'");
		}
		
		if((query_type==QUERY_WORKFLOW_INFO || query_type==QUERY_QUEUE_STATS  || query_type==QUERY_GLOBAL_STATS || query_type==QUERY_SCHEDULER_STATUS) && level>=2)
			throw Exception("SocketQuerySAX2Handler","Unexpected subnode");
		
		if(query_type==QUERY_WORKFLOW_LAUNCH && level==2)
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
		
		if(query_type==QUERY_WORKFLOW_LAUNCH && level>2)
			throw Exception("SocketQuerySAX2Handler","parameter node does not accept subnodes");
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
