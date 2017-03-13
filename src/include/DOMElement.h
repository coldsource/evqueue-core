#ifndef _DOMELEMENT_H_
#define _DOMELEMENT_H_

#include <xercesc/dom/DOM.hpp>
#include <DOMNode.h>

#include <string>

class DOMElement:public DOMNode
{
	xercesc::DOMElement *element;
	
public:
	DOMElement();
	DOMElement(xercesc::DOMElement *element);
	DOMElement(DOMNode node);
	
	bool hasAttribute(const std::string &name);
	std::string getAttribute(const std::string &name);
	void setAttribute(const std::string &name, const std::string &value);
	void removeAttribute(const std::string &name);
};

#endif