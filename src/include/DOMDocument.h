#ifndef _DOMDOCUMENT_H_
#define _DOMDOCUMENT_H_

#include <xercesc/dom/DOM.hpp>
#include <DOMElement.h>
#include <DOMText.h>
#include <DOMXPathResult.h>

#include <string>

class DOMDocument:public DOMNode
{
private:
	xercesc::DOMDocument *xmldoc;
	xercesc::DOMLSParser *parser;
	xercesc::DOMLSSerializer *serializer;
	xercesc::DOMXPathNSResolver *resolver;
	
public:
	DOMDocument(void);
	DOMDocument(xercesc::DOMDocument *xmldoc);
	~DOMDocument(void);
	
	static DOMDocument *Parse(const std::string &xml_str);
	static DOMDocument *ParseFile(const std::string &filename);
	std::string Serialize(DOMNode node);
	std::string ExpandXPathAttribute(const std::string &attribute,DOMNode context_node);
	
	DOMElement getDocumentElement();
	DOMElement createElement(const std::string &name);
	DOMText createTextNode(const std::string &data);
	DOMNode importNode(DOMNode importedNode, bool deep);
	
	DOMXPathResult *evaluate(const std::string &xpath,DOMNode node,DOMXPathResult::ResultType result_type);
};

#endif