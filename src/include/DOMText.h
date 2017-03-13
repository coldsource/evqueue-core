#ifndef _DOMTEXT_H_
#define _DOMTEXT_H_

#include <xercesc/dom/DOM.hpp>
#include <DOMNode.h>

#include <string>

class DOMText:public DOMNode
{
	xercesc::DOMText *text;
	
public:
	DOMText(xercesc::DOMText *text);
};

#endif