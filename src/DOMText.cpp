#include <DOMText.h>

DOMText::DOMText(xercesc::DOMText *text):DOMNode(text)
{
	this->text = text;
}