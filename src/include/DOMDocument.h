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

#ifndef _DOMDOCUMENT_H_
#define _DOMDOCUMENT_H_

#include <xercesc/dom/DOM.hpp>

#include <DOMXPathResult.h>
#include <DOMElement.h>
#include <DOMXPath.h>
#include <DOMText.h>

#include <string>
#include <map>

class DOMDocument:public DOMNode
{
private:
	xercesc::DOMDocument *xmldoc;
	xercesc::DOMLSParser *parser;
	xercesc::DOMLSSerializer *serializer;
	DOMXPath *xpath;
	
	std::map<int,DOMElement> id_node;
	int current_id = -1;
	
public:
	DOMDocument(void);
	DOMDocument(xercesc::DOMDocument *xmldoc);
	~DOMDocument(void);
	
	static DOMDocument *Parse(const std::string &xml_str);
	static DOMDocument *ParseFile(const std::string &filename);
	std::string Serialize(DOMNode node) const;
	std::string ExpandXPathAttribute(const std::string &attribute,DOMNode context_node);
	
	DOMElement getDocumentElement() const;
	DOMXPath *getXPath();
	DOMElement createElement(const std::string &name);
	DOMText createTextNode(const std::string &data);
	DOMNode importNode(DOMNode importedNode, bool deep);
	
	DOMXPathResult *evaluate(const std::string &xpath_str,DOMNode node,DOMXPathResult::ResultType result_type);
	
	std::string getNodeEvqID(DOMElement node);
	DOMElement getNodeFromEvqID(const std::string &evqid);
	
private:
	void initialize_evqid();
};

#endif