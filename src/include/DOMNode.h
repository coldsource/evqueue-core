#ifndef _DOMNODE_H_
#define _DOMNODE_H_

#include <xercesc/dom/DOM.hpp>

#include <string>

class DOMNode
{
	friend class DOMDocument;
	friend class DOMElement;
	
	xercesc::DOMNode *node;
	
public:
	DOMNode();
	DOMNode(xercesc::DOMNode *node);
	
	DOMNode cloneNode(bool deep);
	
	DOMNode getParentNode();
	DOMNode getFirstChild();
	DOMNode appendChild(DOMNode newChild);
	DOMNode removeChild(DOMNode oldChild);
	void replaceChild(DOMNode newChild,DOMNode oldChild);
	DOMNode insertBefore(DOMNode newChild, DOMNode refChild);
	
	std::string getTextContent();
	void setTextContent(const std::string &textContent);
};

#endif