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
#include <XMLString.h>
#include <Exception.h>

#include <xercesc/dom/DOM.hpp>

#include <memory>

using namespace std;

class XMLUtilsErrorHandler: public xercesc::DOMErrorHandler
{
	string errors;
	
	public:
		virtual bool handleError (const xercesc::DOMError &e)
		{
			char *exception_str = xercesc::XMLString::transcode(e.getMessage());
			xercesc::DOMLocator *loc = e.getLocation();
			
			if(errors.length()!=0)
				errors += "\n";
			errors += exception_str;
			errors += " on line "+ std::to_string(loc->getLineNumber());
			
			xercesc::XMLString::release(&exception_str);
		}
		
		const string &GetErrors() { return  errors; }
};

void XMLUtils::ValidateXML(const std::string &xml, const std::string &xsd)
{
	xercesc::DOMImplementation *xercesImplementation = xercesc::DOMImplementationRegistry::getDOMImplementation(XMLString(""));
	xercesc::DOMLSParser *parser = xercesImplementation->createLSParser(xercesc::DOMImplementationLS::MODE_SYNCHRONOUS,0);
	
	XMLUtilsErrorHandler eh;
	parser->getDomConfig()->setParameter(xercesc::XMLUni::fgDOMErrorHandler, &eh);
	
	// Load grammar
	xercesc::DOMLSInput *xsd_input = xercesImplementation->createLSInput();
	
	XMLCh *xsd_xmlch;
	xsd_xmlch = xercesc::XMLString::transcode(xsd.c_str());
	xsd_input->setStringData(xsd_xmlch);
	
	parser->loadGrammar(xsd_input,xercesc::Grammar::SchemaGrammarType,true);
	
	xsd_input->release();
	xercesc::XMLString::release(&xsd_xmlch);
	
	// Load and validate XML
	xercesc::DOMLSInput *xml_input = xercesImplementation->createLSInput();
	
	parser->getDomConfig()->setParameter (xercesc::XMLUni::fgDOMValidate, true);
	parser->getDomConfig()->setParameter (xercesc::XMLUni::fgXercesSchema, true);
	parser->getDomConfig()->setParameter (xercesc::XMLUni::fgXercesSchemaFullChecking, false);
	parser->getDomConfig()->setParameter (xercesc::XMLUni::fgXercesUseCachedGrammarInParse, true);
	parser->getDomConfig()->setParameter (xercesc::XMLUni::fgXercesLoadSchema, false);
	
	XMLCh *xml_xmlch;
	xml_xmlch = xercesc::XMLString::transcode(xml.c_str());
	xml_input->setStringData(xml_xmlch);
	
	xercesc::DOMDocument *xmldoc = parser->parse(xml_input);
	
	xml_input->release();
	xercesc::XMLString::release(&xml_xmlch);
	parser->release();
	
	string errors = eh.GetErrors();
	if(errors.length()!=0)
		throw Exception("XMLValidator",errors.c_str());
}

DOMNode XMLUtils::AppendXML(DOMDocument *xmldoc, DOMNode parent_node, const string &xml)
{
	// Set XML content and parse document
	unique_ptr<DOMDocument> fragment_doc(DOMDocument::Parse(xml));
	if(!fragment_doc)
		throw Exception("XML Parser","Invalid XML document");
	
	DOMNode node = xmldoc->importNode(fragment_doc->getDocumentElement(),true);
	parent_node.appendChild(node);
	
	return node;
}

DOMNode XMLUtils::AppendText(DOMDocument *xmldoc, DOMNode parent_node, const string &text)
{
	// Set XML content and parse document
	DOMElement node = xmldoc->createTextNode(text);
	parent_node.appendChild(node);
	
	return node;
}

string XMLUtils::GetAttribute(DOMElement node, const string &name, bool remove_attribute)
{
	if(!node.hasAttribute(name))
		throw Exception("XMLUtils", "Attribute '"+name+"' does not exist");
	
	string value = node.getAttribute(name);
		
	if(remove_attribute)
		node.removeAttribute(name);
	
	return value;
}

bool XMLUtils::GetAttributeBool(DOMElement node, const string &name, bool remove_attribute)
{
	string val = GetAttribute(node, name, remove_attribute);
	
	if(val=="yes")
		return true;
	else if(val=="no")
		return false;

	try
	{
		return std::stoi(val)?true:false;
	}
	catch(...)
	{
		throw Exception("XML Parser","Attribute '"+name+"' has invalid boolean value");
	}
}

int XMLUtils::GetAttributeInt(DOMElement node, const string &name, bool remove_attribute)
{
	string val = GetAttribute(node, name, remove_attribute);
	
	try
	{
		return std::stoi(val);
	}
	catch(...)
	{
		throw Exception("XML Parser","Attribute '"+name+"' has invalid integer");
	}
}