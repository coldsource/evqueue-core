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

#include <DOMNode.h>
#include <DOMNamedNodeMap.h>

#include <xqilla/xqilla-dom3.hpp>

using namespace std;

DOMNode::DOMNode()
{
	this->node = 0;
}

DOMNode::DOMNode(xercesc::DOMNode *node)
{
	this->node = node;
}

DOMNode DOMNode::cloneNode(bool deep)
{
	return node->cloneNode(deep);
}

DOMNode DOMNode::getParentNode()
{
	return node->getParentNode();
}

DOMNode DOMNode::getFirstChild()
{
	return node->getFirstChild();
}

DOMNode DOMNode::getPreviousSibling()
{
	return node->getPreviousSibling();
}

DOMNode DOMNode::getNextSibling()
{
	return node->getNextSibling();
}

DOMNode DOMNode::appendChild(DOMNode newChild)
{
	return node->appendChild(newChild.node);
}

DOMNode DOMNode::removeChild(DOMNode oldChild)
{
	return node->removeChild(oldChild.node);
}

void DOMNode::replaceChild(DOMNode newChild,DOMNode oldChild)
{
	node->replaceChild(newChild.node,oldChild.node);
	oldChild.node->release();
}

DOMNode DOMNode::insertBefore(DOMNode newChild, DOMNode refChild)
{
	return node->insertBefore(newChild.node,refChild.node);
}

string DOMNode::getNodeName()
{
	char *str = xercesc::XMLString::transcode(node->getNodeName());
	string s(str);
	xercesc::XMLString::release(&str);
	
	return s;
}

string DOMNode::getNodeValue()
{
	char *str = xercesc::XMLString::transcode(node->getNodeValue());
	if(!str)
		str = xercesc::XMLString::transcode(node->getTextContent());
	
	if(!str)
		return "";
	
	string s(str);
	xercesc::XMLString::release(&str);
	
	return s;
}

DOMNode::NodeType DOMNode::getNodeType()
{
	return (NodeType)node->getNodeType();
}

string DOMNode::getTextContent()
{
	char *str = xercesc::XMLString::transcode(node->getTextContent());
	string s(str);
	xercesc::XMLString::release(&str);
	
	return s;
}

void DOMNode::setTextContent(const string &textContent)
{
	node->setTextContent(X(textContent.c_str()));
}

DOMNamedNodeMap DOMNode::getAttributes()
{
	return node->getAttributes();
}

DOMNode::operator bool() const
{
	return node!=0;
}