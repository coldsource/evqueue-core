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

#include <API/XMLMessage.h>
#include <Exception/Exception.h>
#include <DOM/DOMNamedNodeMap.h>

using namespace std;

// Build XML message from socket
XMLMessage::XMLMessage(const string &context, int s)
{
	this->context = context;
	
	xmldoc = new DOMDocument();
	saxh.HandleQuery(s,xmldoc);
	
	init();
}

// Build XML message from buffer
XMLMessage::XMLMessage(const string &context, const string &xml)
{
	this->context = context;
	
	xmldoc = DOMDocument::Parse(xml);
	if(!xmldoc)
		throw Exception(context,"Invalid XML document","INVALID_XML");
	
	init();
}

XMLMessage::XMLMessage::~XMLMessage()
{
	delete xmldoc;
}

void XMLMessage::XMLMessage::init()
{
	DOMNamedNodeMap attributes = xmldoc->getDocumentElement().getAttributes();
	for(int i=0;i<attributes.getLength();i++)
	{
		DOMNode attribute = attributes.item(i);
		string name = attribute.getNodeName();
		string value = attribute.getNodeValue();
		root_attributes[name] = value;
	}
}

bool XMLMessage::HasRootAttribute(const std::string &name)
{
	auto it = root_attributes.find(name);
	if(it==root_attributes.end())
		return false;
	return true;
}

const std::string &XMLMessage::GetRootAttribute(const std::string &name)
{
	auto it = root_attributes.find(name);
	if(it==root_attributes.end())
		throw Exception(context,"Missing '"+name+"' attribute","MISSING_PARAMETER");
	
	return it->second;
}

const std::string &XMLMessage::GetRootAttribute(const std::string &name, const std::string &default_value)
{
	auto it = root_attributes.find(name);
	if(it==root_attributes.end())
		return default_value;
	
	return it->second;
}

int XMLMessage::GetRootAttributeInt(const std::string &name)
{
	auto it = root_attributes.find(name);
	if(it==root_attributes.end())
		throw Exception(context,"Missing '"+name+"' attribute","MISSING_PARAMETER");
	
	try
	{
		return std::stoi(it->second);
	}
	catch(...)
	{
		throw Exception(context,"Attribute '"+name+"' has invalid integer value","INVALID_INTEGER");
	}
}

int XMLMessage::GetRootAttributeInt(const std::string &name, int default_value)
{
	auto it = root_attributes.find(name);
	if(it==root_attributes.end())
		return default_value;

	try
	{
		return std::stoi(it->second);
	}
	catch(...)
	{
		throw Exception(context,"Attribute '"+name+"' has invalid integer value","INVALID_INTEGER");
	}
}

long long XMLMessage::GetRootAttributeLong(const std::string &name)
{
	auto it = root_attributes.find(name);
	if(it==root_attributes.end())
		throw Exception(context,"Missing '"+name+"' attribute","MISSING_PARAMETER");
	
	try
	{
		return std::stoll(it->second);
	}
	catch(...)
	{
		throw Exception(context,"Attribute '"+name+"' has invalid integer value","INVALID_INTEGER");
	}
}

long long XMLMessage::GetRootAttributeLong(const std::string &name, int default_value)
{
	auto it = root_attributes.find(name);
	if(it==root_attributes.end())
		return default_value;

	try
	{
		return std::stoll(it->second);
	}
	catch(...)
	{
		throw Exception(context,"Attribute '"+name+"' has invalid integer value","INVALID_INTEGER");
	}
}

unsigned long long XMLMessage::GetRootAttributeLL(const std::string &name)
{
	auto it = root_attributes.find(name);
	if(it==root_attributes.end())
		throw Exception(context,"Missing '"+name+"' attribute","MISSING_PARAMETER");
	
	try
	{
		return std::stoll(it->second);
	}
	catch(...)
	{
		throw Exception(context,"Attribute '"+name+"' has invalid integer value","INVALID_INTEGER");
	}
}

bool XMLMessage::GetRootAttributeBool(const std::string &name)
{
	auto it = root_attributes.find(name);
	if(it==root_attributes.end())
		throw Exception(context,"Missing '"+name+"' attribute","MISSING_PARAMETER");
	
	if(it->second=="yes")
		return true;
	else if(it->second=="no")
		return false;
	
	try
	{
		return std::stoi(it->second)?true:false;
	}
	catch(...)
	{
		throw Exception(context,"Attribute '"+name+"' has invalid boolean value","INVALID_BOOLEAN");
	}
}

bool XMLMessage::GetRootAttributeBool(const std::string &name, bool default_value)
{
	auto it = root_attributes.find(name);
	if(it==root_attributes.end())
		return default_value;
	
	if(it->second=="yes")
		return true;
	else if(it->second=="no")
		return false;

	try
	{
		return std::stoi(it->second)?true:false;
	}
	catch(...)
	{
		throw Exception(context,"Attribute '"+name+"' has invalid boolean value","INVALID_BOOLEAN");
	}
}
