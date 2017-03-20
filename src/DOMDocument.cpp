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

#include <DOMDocument.h>
#include <DOMXPath.h>
#include <DOMXPathResult.h>
#include <DOMElement.h>
#include <DOMText.h>
#include <Exception.h>

#include <pcrecpp.h>

#include <memory>

#include <xqilla/xqilla-dom3.hpp>

using namespace std;

DOMDocument::DOMDocument(void)
{
	xercesc::DOMImplementation *xqillaImplementation = xercesc::DOMImplementationRegistry::getDOMImplementation(X("XPath2 3.0"));
	xmldoc = xqillaImplementation->createDocument();
	xpath = new DOMXPath(this);
	parser = 0;
	serializer = xqillaImplementation->createLSSerializer();;
	resolver = xmldoc->createNSResolver(xmldoc->getDocumentElement());
	resolver->addNamespaceBinding(X("xs"), X("http://www.w3.org/2001/XMLSchema"));
	
	this->node = (xercesc::DOMNode *)xmldoc;
}

DOMDocument::DOMDocument(xercesc::DOMDocument *xmldoc):DOMNode(xmldoc)
{
	this->xmldoc = xmldoc;
	xpath = new DOMXPath(this);
	parser = 0;
	serializer = 0;
	resolver = 0;
}

DOMDocument::~DOMDocument(void)
{
	// Document has not been parsed, we have to relase xmldoc explicitly
	if(!parser && xmldoc)
		xmldoc->release();
	
	if(xpath)
		delete xpath;
	
	// Releaseing parser will free xmldoc for parsed documents
	if(parser)
		parser->release();

	if(serializer)
		serializer->release();
}

DOMDocument *DOMDocument::Parse(const string &xml_str)
{
	DOMDocument *doc = new DOMDocument(0);
	
	xercesc::DOMImplementation *xqillaImplementation = xercesc::DOMImplementationRegistry::getDOMImplementation(X("XPath2 3.0"));
	doc->parser = xqillaImplementation->createLSParser(xercesc::DOMImplementationLS::MODE_SYNCHRONOUS,0);
	doc->serializer = xqillaImplementation->createLSSerializer();

	xercesc::DOMLSInput *input = xqillaImplementation->createLSInput();

	// Set XML content and parse document
	XMLCh *xml;
	xml = xercesc::XMLString::transcode(xml_str.c_str());
	
	input->setStringData(xml);
	doc->xmldoc = doc->parser->parse(input);
	doc->node = doc->xmldoc;
	input->release();
	
	xercesc::XMLString::release(&xml);
	
	doc->resolver = doc->xmldoc->createNSResolver(doc->xmldoc->getDocumentElement());
	doc->resolver->addNamespaceBinding(X("xs"), X("http://www.w3.org/2001/XMLSchema"));
	
	if(!doc->xmldoc->getDocumentElement())
	{
		// Parse error
		delete doc;
		return 0;
	}
	
	return doc;
}

DOMDocument *DOMDocument::ParseFile(const string &filename)
{
	DOMDocument *doc = new DOMDocument(0);
	
	xercesc::DOMImplementation *xqillaImplementation = xercesc::DOMImplementationRegistry::getDOMImplementation(X("XPath2 3.0"));
	doc->parser = xqillaImplementation->createLSParser(xercesc::DOMImplementationLS::MODE_SYNCHRONOUS,0);
	
	// Load XML from file
	try
	{
		doc->xmldoc = doc->parser->parseURI(X(filename.c_str()));
	}
	catch(...)
	{
		delete doc;
		return 0;
	}
	
	if(!doc->xmldoc || !doc->xmldoc->getDocumentElement())
	{
		delete doc;
		return 0;
	}
	
	return doc;
}

string DOMDocument::Serialize(DOMNode node)
{
	XMLCh *xml_output = serializer->writeToString(node.node);
	char *xml_output_c = xercesc::XMLString::transcode(xml_output);
	
	string s(xml_output_c);
	
	xercesc::XMLString::release(&xml_output);
	xercesc::XMLString::release(&xml_output_c);
	
	return s;
}

string DOMDocument::ExpandXPathAttribute(const string &attribute,DOMNode context_node)
{
	string attribute_expanded = attribute;
	
	pcrecpp::RE regex("(\\{[^\\}]+\\})");
	pcrecpp::StringPiece str_to_match(attribute);
	std::string match;
	while(regex.FindAndConsume(&str_to_match,&match))
	{
		string xpath_piece_raw = match;
		string xpath_piece =  xpath_piece_raw.substr(1,xpath_piece_raw.length()-2);
		
		unique_ptr<DOMXPathResult> value_nodes(evaluate(xpath_piece,context_node,DOMXPathResult::FIRST_RESULT_TYPE));
		
		if(value_nodes->isNode())
		{
			DOMNode value_node = value_nodes->getNodeValue();
			
			size_t start_pos = attribute_expanded.find(xpath_piece_raw);
			
			string expanded_value = value_node.getTextContent();
			attribute_expanded.replace(start_pos,xpath_piece_raw.length(),expanded_value);
		}
	}
	
	return attribute_expanded;
}

DOMElement DOMDocument::getDocumentElement()
{
	return xmldoc->getDocumentElement();
}

DOMXPath *DOMDocument::getXPath()
{
	return xpath;
}

DOMElement DOMDocument::createElement(const string &name)
{
	return xmldoc->createElement(X(name.c_str()));
}

DOMText DOMDocument::createTextNode(const string &data)
{
	return xmldoc->createTextNode(X(data.c_str()));
}

DOMNode DOMDocument::importNode(DOMNode importedNode, bool deep)
{
	return xmldoc->importNode(importedNode.node,deep);
}

DOMXPathResult *DOMDocument::evaluate(const string &xpath_str,DOMNode node,DOMXPathResult::ResultType result_type)
{
	try
	{
		return xpath->evaluate(xpath_str,node,result_type);
	}
	catch(Exception &e)
	{
		throw Exception("DOMDocument","Error evaluating XPath expression : "+xpath_str+". XPath returned error : "+e.error+" in context "+e.context);
	}
}