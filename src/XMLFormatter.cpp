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
#include <DOMNamedNodeMap.h>

using namespace std;

XMLFormatter::XMLFormatter(const string& xml_str)
{
	xmldoc = DOMDocument::Parse(xml_str);
}

XMLFormatter::~XMLFormatter()
{
	if(xmldoc)
		delete xmldoc;
}

void XMLFormatter::Format()
{
	level = 0;
	format(xmldoc->getDocumentElement());
}

void XMLFormatter::format(DOMNode node)
{
	level++;
	
	do
	{
		short node_type = node.getNodeType();
		if(node_type==DOMNode::ELEMENT_NODE)
		{
			display_element_start(node);
			
			DOMNode child = node.getFirstChild();
			if(child)
			{
				printf(">\n");
				format(child);
				display_element_end(node);
			}
			else
				printf("/>\n");
		}
		else if(node_type==DOMNode::TEXT_NODE)
			display_text(node);
	}while(node = node.getNextSibling());
	
	level--;
}

void XMLFormatter::display_element_start(DOMElement element)
{
	string node_name = element.getNodeName();
	
	// Padding
	for(int i=0;i<level-1;i++)
		printf("  ");
	
	// Node name
	printf("<%s",node_name.c_str());
	
	// Attributes
	DOMNamedNodeMap attributes = element.getAttributes();
	for(int i=0;i<attributes.getLength();i++)
	{
		DOMNode attribute = attributes.item(i);
		string name = attribute.getNodeName();
		string value = attribute.getNodeValue();
		printf(" %s=\"%s\"",name.c_str(),value.c_str());
	}
}

void XMLFormatter::display_element_end(DOMElement element)
{
	string node_name = element.getNodeName();
	
	// Padding
	for(int i=0;i<level-1;i++)
		printf("  ");
	
	// Node name
	printf("</%s>\n",node_name.c_str());
}

void XMLFormatter::display_text(DOMNode node)
{
	string text = node.getNodeValue();
	
	// Text
	int len = text.length();
	
	// Trim left
	int first = -1;
	for(int i=0;i<len;i++)
	{
		if(text[i]!='\r' && text[i]!='\n' && text[i]!=' ' && text[i]!='\t')
		{
			first = i;
			break;
		}
	}
	
	int last = -1;
	for(int i=len-1;i>=0;i--)
	{
		if(text[i]!='\r' && text[i]!='\n' && text[i]!=' ' && text[i]!='\t')
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
		
		fwrite(text.c_str()+first,1,last-first+1,stdout);
		printf("\n");
	}
}