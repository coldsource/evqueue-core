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

#include <SocketSAX2Handler.h>
#include <NetworkInputSource.h>
#include <Logger.h>
#include <Exception.h>

#include <xercesc/sax2/SAX2XMLReader.hpp>
#include <xercesc/sax2/XMLReaderFactory.hpp>
#include <xercesc/sax2/DefaultHandler.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>
#include <xercesc/framework/Wrapper4InputSource.hpp>

using namespace xercesc;

SocketSAX2Handler::SocketSAX2Handler(int socket)
{
	this->socket = socket;
}

void SocketSAX2Handler::HandleQuery(SocketSAX2HandlerInterface *handler)
{
	SAX2XMLReader *parser = 0;
	
	try
	{
		parser = XMLReaderFactory::createXMLReader();
		parser->setFeature(XMLUni::fgSAX2CoreValidation, true);
		parser->setFeature(XMLUni::fgSAX2CoreNameSpaces, true);
		
		XMLSize_t lowWaterMark = 0;
		parser->setProperty(XMLUni::fgXercesLowWaterMark, &lowWaterMark);
		
		parser->setContentHandler(handler);
		parser->setErrorHandler(handler);
		
		// Create a progressive scan token
		XMLPScanToken token;
		NetworkInputSource source(socket);
		
		try
		{
			if (!parser->parseFirst(source, token))
			{
				Logger::Log(LOG_ERR,"parseFirst failed");
				throw Exception("core","parseFirst failed");
			}
			
			bool gotMore = true;
			while (gotMore && !handler->IsReady()) {
				gotMore = parser->parseNext(token);
			}
		}
		catch (const SAXParseException& toCatch)
		{
			char *message = XMLString::transcode(toCatch.getMessage());
			Logger::Log(LOG_WARNING,"Invalid query XML structure : %s",message);
			XMLString::release(&message);
			
			throw Exception("Query XML parsing","Invalid query XML structure");
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
		Logger::Log(LOG_WARNING,"Unexpected exception in context %s : %s\n",e.context,e.error);
		
		if (parser!=0)
			delete parser;
		
		throw e;
	}
	
	if (parser!=0)
		delete parser;
}