#include <DOMNode.h>

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