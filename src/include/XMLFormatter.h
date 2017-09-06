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

#ifndef _XMLFORMATTER_H_
#define _XMLFORMATTER_H_

#include <DOMDocument.h>

#include <string>

class XMLFormatter
{
	DOMDocument *xmldoc = 0;
	bool external_xmldoc;
	
	int level;
	bool display_output;
	std::string output;
	
	public:
		XMLFormatter(DOMDocument *xmldoc);
		XMLFormatter(const std::string &xml_str);
		~XMLFormatter();
		
		void Format(bool display_output = true);
		std::string GetOutput() { return output; }
	
	private:
		void format(DOMNode node);
		void display_element_start(DOMElement element);
		void display_element_end(DOMElement element);
		void display_text(DOMNode node);
};

#endif