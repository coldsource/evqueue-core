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

#ifndef _DOMNODE_H_
#define _DOMNODE_H_

#include <xercesc/dom/DOM.hpp>

#include <string>

class DOMNamedNodeMap;

class DOMNode
{
	friend class DOMDocument;
	friend class DOMElement;
	
	xercesc::DOMNode *node;
	
public:
	enum NodeType {
		ELEMENT_NODE                = 1,
		ATTRIBUTE_NODE              = 2,
		TEXT_NODE                   = 3,
		CDATA_SECTION_NODE          = 4,
		ENTITY_REFERENCE_NODE       = 5,
		ENTITY_NODE                 = 6,
		PROCESSING_INSTRUCTION_NODE = 7,
		COMMENT_NODE                = 8,
		DOCUMENT_NODE               = 9,
		DOCUMENT_TYPE_NODE          = 10,
		DOCUMENT_FRAGMENT_NODE      = 11,
		NOTATION_NODE               = 12
	};
	
	DOMNode();
	DOMNode(xercesc::DOMNode *node);
	
	DOMNode cloneNode(bool deep);
	
	DOMNode getParentNode();
	DOMNode getFirstChild();
	DOMNode getNextSibling();
	DOMNode appendChild(DOMNode newChild);
	DOMNode removeChild(DOMNode oldChild);
	void replaceChild(DOMNode newChild,DOMNode oldChild);
	DOMNode insertBefore(DOMNode newChild, DOMNode refChild);
	
	std::string getNodeName();
	std::string getNodeValue();
	NodeType getNodeType();
	std::string getTextContent();
	void setTextContent(const std::string &textContent);
	
	DOMNamedNodeMap getAttributes();
	
	operator bool() const;
};

#endif