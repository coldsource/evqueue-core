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

#ifndef _DOMXPATHRESULT_H_
#define _DOMXPATHRESULT_H_

#include <DOM/DOMNode.h>

#include <string>

class Token;

class DOMXPathResult
{
	Token *result;
	int idx;
	
public:
	enum ResultType {
		ANY_TYPE = 0, NUMBER_TYPE = 1, STRING_TYPE = 2, BOOLEAN_TYPE = 3,
		UNORDERED_NODE_ITERATOR_TYPE = 4, ORDERED_NODE_ITERATOR_TYPE = 5, UNORDERED_NODE_SNAPSHOT_TYPE = 6, ORDERED_NODE_SNAPSHOT_TYPE = 7,
		ANY_UNORDERED_NODE_TYPE = 8, FIRST_ORDERED_NODE_TYPE = 9, FIRST_RESULT_TYPE = 100, ITERATOR_RESULT_TYPE = 101,
		SNAPSHOT_RESULT_TYPE = 102
	};
	
	DOMXPathResult(Token *result);
	~DOMXPathResult();

	bool snapshotItem(int index);
	unsigned int length();
	
	ResultType GetNodeType();
	bool isNode();
	DOMNode getNodeValue();
	
	int getIntegerValue();
	bool getBooleanValue();
	std::string getStringValue();
};

#endif
