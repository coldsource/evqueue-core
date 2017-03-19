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

bool DOMElement::hasAttribute(const string &name) const
{
	return element->hasAttribute(X(name.c_str()));
}

string DOMElement::getAttribute(const string &name) const
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