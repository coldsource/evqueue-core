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
#include <DOMDocument.h>

#include <string>

class XMLUtils
{
	public:
		static void ValidateXML(const std::string &xml, const std::string &xsd);
		static DOMNode AppendXML(DOMDocument *xmldoc, DOMNode parent_node, const std::string &xml);
		static std::string GetAttribute(DOMElement node, const std::string &name, bool remove_attribute = false);
		static bool GetAttributeBool(DOMElement node, const std::string &name, bool remove_attribute = false);
		static int GetAttributeInt(DOMElement node, const std::string &name, bool remove_attribute = false);
};