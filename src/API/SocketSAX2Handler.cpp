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

#include <API/SocketSAX2Handler.h>
#include <IO/NetworkInputSource.h>
#include <Exception/Exception.h>
#include <XML/XMLString.h>

#include <xercesc/sax2/Attributes.hpp>
#include <xercesc/dom/DOM.hpp>
#include <xercesc/sax2/SAX2XMLReader.hpp>
#include <xercesc/sax2/XMLReaderFactory.hpp>
#include <xercesc/sax2/DefaultHandler.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>
#include <xercesc/framework/Wrapper4InputSource.hpp>

using namespace std;

SocketSAX2Handler::SocketSAX2Handler()
{
	level = 0;
	ready = false;
}

void SocketSAX2Handler::HandleQuery(int socket, DOMDocument *xmldoc)
{
	xercesc::SAX2XMLReader *parser = 0;
	this->xmldoc = xmldoc;
	
	try
	{
		parser = xercesc::XMLReaderFactory::createXMLReader();
		parser->setFeature(xercesc::XMLUni::fgSAX2CoreValidation, true);
		parser->setFeature(xercesc::XMLUni::fgSAX2CoreNameSpaces, true);
		
		XMLSize_t lowWaterMark = 0;
		parser->setProperty(xercesc::XMLUni::fgXercesLowWaterMark, &lowWaterMark);
		
		parser->setContentHandler(this);
		parser->setErrorHandler(this);
		
		// Create a progressive scan token
		xercesc::XMLPScanToken token;
		NetworkInputSource source(socket);
		
		try
		{
			if (!parser->parseFirst(source, token))
				throw Exception("core","parseFirst failed");
			
			bool gotMore = true;
			while (gotMore && !this->IsReady()) {
				gotMore = parser->parseNext(token);
			}
		}
		catch (const xercesc::SAXParseException& toCatch)
		{
			char *message = xercesc::XMLString::transcode(toCatch.getMessage());
			string excpt_msg = "Invalid query XML structure : ";
			excpt_msg.append(message);
			xercesc::XMLString::release(&message);
			
			throw Exception("Query XML parsing",excpt_msg);
		}
		catch(Exception &e)
		{
			throw e;
		}
		catch(int e)  // int exception thrown to indicate that we have received a complete XML (usual case)
		{
			; // nothing to do, just let the code roll
		}
		catch (...)
		{
			throw Exception("Query XML parsing","Unexpected error trying to parse query XML");
		}
	}
	catch (Exception &e)
	{
		if (parser!=0)
			delete parser;
		
		throw e;
	}
	
	if (parser!=0)
		delete parser;
}

void SocketSAX2Handler::startElement(const XMLCh* const uri, const XMLCh* const localname, const XMLCh* const qname, const xercesc::Attributes& attrs)
{
	XMLString node_name(localname);
	
	level++;
	
	// Store XML in DOM document, recreating all elements
	DOMElement node = xmldoc->createElement(node_name);
	
	for(int i=0;i<attrs.getLength();i++)
	{
		const XMLCh *attr_name, *attr_value;
		attr_name = attrs.getLocalName(i);
		attr_value = attrs.getValue(i);
		
		node.setAttribute(XMLString(attr_name),XMLString(attr_value));
	}
	
	if(level==1)
		xmldoc->appendChild(node);
	else
		current_node.at(level-2).appendChild(node);
	
	current_node.push_back(node);
}

void SocketSAX2Handler::endElement (const XMLCh *const uri, const XMLCh *const localname, const XMLCh *const qname)
{
	level--;
	
	current_node.pop_back();
	
	if (level==0) {
		ready = true;
		throw 0;  // get out of the parseNext loop
	}
}

void SocketSAX2Handler::characters(const XMLCh *const chars, const XMLSize_t length)
{
	XMLCh *chars_nt = new XMLCh[length+1];
	memcpy(chars_nt,chars,length*sizeof(XMLCh));
	chars_nt[length] = 0;
	
	current_text_node = xmldoc->createTextNode(XMLString(chars_nt));
	current_node.at(level-1).appendChild(current_text_node);
	
	delete[] chars_nt;
}

void SocketSAX2Handler::endDocument ()
{
	ready = true;
}
