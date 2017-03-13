#include <DOMElement.h>

#include <xqilla/xqilla-dom3.hpp>

using namespace std;

DOMElement::DOMElement()
{
	this->element = 0;
}

DOMElement::DOMElement(xercesc::DOMElement *element):DOMNode(element)
{
	this->element = element;
}

DOMElement::DOMElement(DOMNode node):DOMNode(node.node)
{
	this->element = (xercesc::DOMElement *)node.node;
}

bool DOMElement::hasAttribute(const string &name)
{
	return element->hasAttribute(X(name.c_str()));
}

string DOMElement::getAttribute(const string &name)
{
	char *str = xercesc::XMLString::transcode(element->getAttribute(X(name.c_str())));
	string s(str);
	xercesc::XMLString::release(&str);
	
	return s;
}

void DOMElement::setAttribute(const string &name, const string &value)
{
	element->setAttribute(X(name.c_str()),X(value.c_str()));
}

void DOMElement::removeAttribute(const string &name)
{
	element->removeAttribute(X(name.c_str()));
}