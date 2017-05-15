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

#include <XMLString.h>

using namespace std;

XMLString::XMLString(const string &str)
{
	this->xmlstr = xercesc::XMLString::transcode(str.c_str());
	this->str = str;
}

XMLString::XMLString(const XMLCh *xmlstr)
{
	char *ptr = xercesc::XMLString::transcode(xmlstr);
	this->str = ptr;
	xercesc::XMLString::release(&ptr);
	
	this->xmlstr = xercesc::XMLString::transcode(this->str.c_str());
}

XMLString::XMLString(const XMLString &xmlstr)
{
	this->str = xmlstr.str;
	this->xmlstr = xercesc::XMLString::transcode(this->str.c_str());
}

XMLString::~XMLString()
{
	xercesc::XMLString::release(&xmlstr);
}

XMLString::operator const XMLCh*() const
{
	return xmlstr;
}

XMLString::operator const string &() const
{
	return str;
}