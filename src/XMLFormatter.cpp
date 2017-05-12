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

#include <XMLFormatter.h>
#include <XMLString.h>

using namespace std;

XMLFormatter::XMLFormatter(const string& xml_str)
{
	xercesc::DOMImplementation *xercesImplementation = xercesc::DOMImplementationRegistry::getDOMImplementation(XMLString(""));
	parser = xercesImplementation->createLSParser(xercesc::DOMImplementationLS::MODE_SYNCHRONOUS,0);
	
	xercesc::DOMLSInput *input = xercesImplementation->createLSInput();
	
	// Set XML content and parse document
	XMLCh *xml;
	xml = xercesc::XMLString::transcode(xml_str.c_str());
	input->setStringData(xml);
	
	xmldoc = parser->parse(input);

	input->release();
	xercesc::XMLString::release(&xml);
}

XMLFormatter::~XMLFormatter()
{
	if(parser)
		parser->release();
}

void XMLFormatter::Format()
{
	level = 0;
	format(xmldoc->getDocumentElement());
}

void XMLFormatter::format(xercesc::DOMNode* node)
{
	level++;
	
	do
	{
		short node_type = node->getNodeType();
		if(node_type==xercesc::DOMNode::ELEMENT_NODE)
		{
			display_element_start((xercesc::DOMElement *)node);
			
			xercesc::DOMNode *child = (xercesc::DOMElement *)node->getFirstChild();
			if(child)
			{
				printf(">\n");
				format(child);
				display_element_end((xercesc::DOMElement *)node);
			}
			else
				printf("/>\n");
		}
		else if(node_type==xercesc::DOMNode::TEXT_NODE)
			display_text(node);
	}while(node = node->getNextSibling());
	
	level--;
}

void XMLFormatter::display_element_start(xercesc::DOMElement *element)
{
	const XMLCh *node_name = element->getNodeName();
	char *node_name_c = xercesc::XMLString::transcode(node_name);
	
	// Padding
	for(int i=0;i<level-1;i++)
		printf("  ");
	
	// Node name
	printf("<%s",node_name_c);
	
	// Attributes
	xercesc::DOMNamedNodeMap *attributes = element->getAttributes();
	for(int i=0;i<attributes->getLength();i++)
	{
		xercesc::DOMNode *attribute = attributes->item(i);
		char *name_c = xercesc::XMLString::transcode(attribute->getNodeName());
		char *value_c = xercesc::XMLString::transcode(attribute->getNodeValue());
		printf(" %s=\"%s\"",name_c,value_c);
		xercesc::XMLString::release(&name_c);
		xercesc::XMLString::release(&value_c);
	}
	
	xercesc::XMLString::release(&node_name_c);
}

void XMLFormatter::display_element_end(xercesc::DOMElement *element)
{
	const XMLCh *node_name = element->getNodeName();
	char *node_name_c = xercesc::XMLString::transcode(node_name);
	
	// Padding
	for(int i=0;i<level-1;i++)
		printf("  ");
	
	// Node name
	printf("</%s>\n",node_name_c);
	
	xercesc::XMLString::release(&node_name_c);
}

void XMLFormatter::display_text(xercesc::DOMNode *node)
{
	const XMLCh *text = node->getNodeValue();
	char *text_c = xercesc::XMLString::transcode(text);
	
	// Text
	int len = strlen(text_c);
	
	// Trim left
	int first = -1;
	for(int i=0;i<len;i++)
	{
		if(text_c[i]!='\r' && text_c[i]!='\n' && text_c[i]!=' ' && text_c[i]!='\t')
		{
			first = i;
			break;
		}
	}
	
	int last = -1;
	for(int i=len-1;i>=0;i--)
	{
		if(text_c[i]!='\r' && text_c[i]!='\n' && text_c[i]!=' ' && text_c[i]!='\t')
		{
			last = i;
			break;
		}
	}
	
	if(first!=-1 && last!=-1 && first<=last)
	{
		// Padding
		for(int i=0;i<level-1;i++)
			printf("  ");
		
		fwrite(text_c+first,1,last-first+1,stdout);
		printf("\n");
	}
	
	xercesc::XMLString::release(&text_c);
}