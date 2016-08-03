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

#include <XMLUtils.h>
#include <Exception.h>

#include <xqilla/xqilla-dom3.hpp>
#include <xercesc/dom/DOM.hpp>

using namespace xercesc;
using namespace std;

class XMLUtilsErrorHandler: public DOMErrorHandler
{
	string errors;
	
	public:
		virtual bool handleError (const xercesc::DOMError &e)
		{
			char *exception_str = XMLString::transcode(e.getMessage());
			DOMLocator *loc = e.getLocation();
			
			if(errors.length()!=0)
				errors += "\n";
			errors += exception_str;
			errors += " on line "+ std::to_string(loc->getLineNumber());
			
			XMLString::release(&exception_str);
		}
		
		const string &GetErrors() { return  errors; }
};

void XMLUtils::ValidateXML(const std::string &xml, const std::string &xsd)
{
	DOMImplementation *xqillaImplementation = DOMImplementationRegistry::getDOMImplementation(X("XPath2 3.0"));
	DOMLSParser *parser = xqillaImplementation->createLSParser(DOMImplementationLS::MODE_SYNCHRONOUS,0);
	
	XMLUtilsErrorHandler eh;
	parser->getDomConfig()->setParameter(XMLUni::fgDOMErrorHandler, &eh);
	
	// Load grammar
	DOMLSInput *xsd_input = xqillaImplementation->createLSInput();
	
	XMLCh *xsd_xmlch;
	xsd_xmlch = XMLString::transcode(xsd.c_str());
	xsd_input->setStringData(xsd_xmlch);
	
	parser->loadGrammar(xsd_input,Grammar::SchemaGrammarType,true);
	
	xsd_input->release();
	XMLString::release(&xsd_xmlch);
	
	// Load and validate XML
	DOMLSInput *xml_input = xqillaImplementation->createLSInput();
	
	parser->getDomConfig()->setParameter (XMLUni::fgDOMValidate, true);
	parser->getDomConfig()->setParameter (XMLUni::fgXercesSchema, true);
	parser->getDomConfig()->setParameter (XMLUni::fgXercesSchemaFullChecking, false);
	parser->getDomConfig()->setParameter (XMLUni::fgXercesUseCachedGrammarInParse, true);
	parser->getDomConfig()->setParameter (XMLUni::fgXercesLoadSchema, false);
	
	XMLCh *xml_xmlch;
	xml_xmlch = XMLString::transcode(xml.c_str());
	xml_input->setStringData(xml_xmlch);
	
	DOMDocument *xmldoc = parser->parse(xml_input);
	
	xml_input->release();
	XMLString::release(&xml_xmlch);
	parser->release();
	
	string errors = eh.GetErrors();
	if(errors.length()!=0)
		throw Exception("XMLValidator",errors.c_str());
}