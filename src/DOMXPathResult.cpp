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

#include <DOMXPathResult.h>
#include <Exception.h>
#include <XPathTokens.h>

using namespace std;

DOMXPathResult::DOMXPathResult(Token *result)
{
	this->result = result;
	idx = -1;
}

DOMXPathResult::~DOMXPathResult()
{
	delete result;
}

bool DOMXPathResult::snapshotItem(int index)
{
	if(result->GetType()==NODELIST)
	{
		TokenNodeList *list = (TokenNodeList *)result;
		if(index<list->nodes.size())
		{
			idx = index;
			return true;
		}
		return false;
	}
	
	// We have litteral result, only one result is possible
	if(index==0)
	{
		idx = 0;
		return true;
	}
	
	return  false;
}

bool DOMXPathResult::isNode()
{
	return (idx!=-1);
}

DOMNode DOMXPathResult::getNodeValue()
{
	if(result->GetType()==NODELIST)
	{
		TokenNodeList *list = (TokenNodeList *)result;
		return list->nodes.at(idx);
	}
	
	throw Exception("DOMXPathResult","Evaluation result is not a node");
}

int DOMXPathResult::getIntegerValue()
{
	if(result->GetType()==NODELIST)
	{
		TokenNodeList *list = (TokenNodeList *)result;
		try
		{
			return stoi(list->nodes.at(idx).getNodeValue());
		}
		catch(...)
		{
			throw Exception("DOMXPathResult","Could not cast result to integer");
		}
	}
	else
		return (int)(*result);
}

string DOMXPathResult::getStringValue()
{
	if(result->GetType()==NODELIST)
	{
		TokenNodeList *list = (TokenNodeList *)result;
		try
		{
			return list->nodes.at(idx).getNodeValue();
		}
		catch(...)
		{
			throw Exception("DOMXPathResult","Could not cast result to integer");
		}
	}
	else
		return (string)(*result);
}