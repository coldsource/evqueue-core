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

#include <DOM/DOMDocument.h>
#include <DOM/DOMXPath.h>
#include <DOM/DOMXPathResult.h>
#include <DOM/DOMElement.h>
#include <DOM/DOMText.h>
#include <Exception/Exception.h>
#include <XML/XMLString.h>
#include <Logger/Logger.h>

#include <regex>
#include <memory>

using namespace std;

DOMDocument::DOMDocument(void)
{
	xercesc::DOMImplementation *xercesImplementation = xercesc::DOMImplementationRegistry::getDOMImplementation(XMLString(""));
	xmldoc = xercesImplementation->createDocument();
	xpath = new DOMXPath(this);
	parser = 0;
	serializer = xercesImplementation->createLSSerializer();
	
	this->node = (xercesc::DOMNode *)xmldoc;
}

DOMDocument::DOMDocument(xercesc::DOMDocument *xmldoc):DOMNode(xmldoc)
{
	xercesc::DOMImplementation *xercesImplementation = xercesc::DOMImplementationRegistry::getDOMImplementation(XMLString(""));
	this->xmldoc = xmldoc;
	xpath = new DOMXPath(this);
	parser = 0;
	serializer = xmldoc?xercesImplementation->createLSSerializer():0;
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
	
	xercesc::DOMImplementation *xercesImplementation = xercesc::DOMImplementationRegistry::getDOMImplementation(XMLString(""));
	doc->parser = xercesImplementation->createLSParser(xercesc::DOMImplementationLS::MODE_SYNCHRONOUS,0);
	doc->serializer = xercesImplementation->createLSSerializer();

	xercesc::DOMLSInput *input = xercesImplementation->createLSInput();

	// Set XML content and parse document
	XMLCh *xml;
	xml = xercesc::XMLString::transcode(xml_str.c_str());
	
	input->setStringData(xml);
	doc->xmldoc = doc->parser->parse(input);
	doc->node = doc->xmldoc;
	input->release();
	
	xercesc::XMLString::release(&xml);
	
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
	
	xercesc::DOMImplementation *xercesImplementation = xercesc::DOMImplementationRegistry::getDOMImplementation(XMLString(""));
	doc->parser = xercesImplementation->createLSParser(xercesc::DOMImplementationLS::MODE_SYNCHRONOUS,0);
	doc->serializer = xercesImplementation->createLSSerializer();
	
	// Load XML from file
	XMLCh *xfilename = xercesc::XMLString::transcode(filename.c_str());
	try
	{
		doc->xmldoc = doc->parser->parseURI(xfilename);
		xercesc::XMLString::release(&xfilename);
	}
	catch(...)
	{
		xercesc::XMLString::release(&xfilename);
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

string DOMDocument::Serialize(DOMNode node) const
{
	XMLCh *xml_output = serializer->writeToString(node.node);
	char *xml_output_c = xercesc::XMLString::transcode(xml_output);
	
	if(!xml_output || !xml_output_c)
	{
#ifdef BUILD_MODULE_EVQUEUE_CORE
		Logger::Log(LOG_ERR, "Unable to serialize XML document, document might contain invalid UTF8 character");
#endif
		
		throw Exception("DOMDocument", "Unable to serialize XML document");
	}
	
	string s(xml_output_c);
	
	xercesc::XMLString::release(&xml_output);
	xercesc::XMLString::release(&xml_output_c);
	
	return s;
}

string DOMDocument::ExpandXPathAttribute(const string &attribute,DOMNode context_node)
{
	string attribute_expanded = attribute;
	
	regex attr_regex("(\\{[^\\}]+\\})");
	
	auto words_begin = sregex_iterator(attribute.begin(), attribute.end(), attr_regex);
	auto words_end = sregex_iterator();
	for(auto i=words_begin;i!=words_end;++i)
	{
		string xpath_piece_raw = i->str();
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

DOMElement DOMDocument::getDocumentElement() const
{
	return xmldoc->getDocumentElement();
}

DOMXPath *DOMDocument::getXPath()
{
	return xpath;
}

DOMElement DOMDocument::createElement(const string &name)
{
	return xmldoc->createElement(XMLString(name.c_str()));
}

DOMText DOMDocument::createTextNode(const string &data)
{
	return xmldoc->createTextNode(XMLString(data.c_str()));
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
		throw Exception("DOMDocument","XPath expression error in '"+xpath_str+"'. XPath returned error : "+e.error+" ("+e.context+")");
	}
}

string DOMDocument::getNodeEvqID(DOMElement node)
{
	if(current_id==-1)
		initialize_evqid();
	
	try
	{
		string evqid;
		string attribute_name;
		
		if(node.getNodeType()==ATTRIBUTE_NODE)
		{
			attribute_name = node.getNodeName();
			node = node.getOwnerElement();
		}
		
		if(node.hasAttribute("evqid"))
			evqid = node.getAttribute("evqid");
		else
		{
			node.setAttribute("evqid",to_string(++current_id));
			id_node[current_id] = node;
			evqid = to_string(current_id);
		}
		
		if(attribute_name!="")
			evqid += "/@"+attribute_name;
		
		return evqid;
	}
	catch(...)
	{
		// Catch exceptions stoi() could throw
		throw Exception("DOMDocument","Invalid evqid found");
	}
}

DOMElement DOMDocument::getNodeFromEvqID(const string &evqid)
{
	if(current_id==-1)
		initialize_evqid();
	
	int ievqid;
	string attribute_name;
	
	try
	{
		size_t pos = evqid.find("/@");
		if(pos!=string::npos)
		{
			// This is an attribute
			ievqid = stoi(evqid.substr(0,pos));
			attribute_name = evqid.substr(pos+2);
			if(attribute_name=="")
				throw -1;
		}
		else
		{
			// This is a node
			ievqid = stoi(evqid);
		}
	}
	catch(...)
	{
		throw Exception("DOMDocument","Invalid context ID");
	}
	
	auto it = id_node.find(ievqid);
	if(it==id_node.end())
		return DOMElement();
	
	if(attribute_name=="")
		return it->second; // Node
	
	// Attribute
	return (it->second).getAttributeNode(attribute_name);
}

void DOMDocument::ImportXPathResult(DOMXPathResult *res, DOMNode node)
{
	if(res->GetNodeType()==DOMXPathResult::NUMBER_TYPE)
		node.appendChild(createTextNode(to_string(res->getIntegerValue())));
	else if(res->GetNodeType()==DOMXPathResult::STRING_TYPE)
		node.appendChild(createTextNode(res->getStringValue()));
	else if(res->GetNodeType()==DOMXPathResult::BOOLEAN_TYPE)
		node.appendChild(createTextNode(res->getIntegerValue()?"true":"false"));
	else if(res->GetNodeType()==DOMXPathResult::ITERATOR_RESULT_TYPE)
	{
		unsigned int index = 0;
		while(res->snapshotItem(index++))
		{
			if(res->getNodeValue().getNodeType()==ATTRIBUTE_NODE)
				node.appendChild(createTextNode(res->getNodeValue().getTextContent()));
			else
				node.appendChild(importNode(res->getNodeValue(),true));
		}
	}
	else
		node.appendChild(createTextNode("unknown"));
}

void DOMDocument::initialize_evqid()
{
	current_id = 0;
	
	try
	{
		// Look for nodes hyaving an evqid attribute
		unique_ptr<DOMXPathResult> res(evaluate("//*[count(@evqid) = 1]",getDocumentElement(),DOMXPathResult::FIRST_RESULT_TYPE));
		int i = 0;
		while(res->snapshotItem(i++))
		{
			DOMElement node = (DOMElement)res->getNodeValue();
			int val = stoi(node.getAttribute("evqid"));
			
			// Update map and current_id
			id_node[val] = node;
			
			if(val>current_id)
				current_id = val;
		}
	}
	catch(...)
	{
		// Catch exceptions stoi() could throw
		throw Exception("DOMDocument","Invalid evqid found");
	}
}
