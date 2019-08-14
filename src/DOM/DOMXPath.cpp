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

#include <DOMXPath.h>
#include <DOMXPathResult.h>
#include <XPathEval.h>
#include <Exception.h>

DOMXPath::DOMXPath(DOMDocument *xmldoc):eval(xmldoc)
{
	this->xmldoc = xmldoc;
}

void DOMXPath::RegisterFunction(std::string name,XPathEval::func_desc f)
{
	eval.RegisterFunction(name,f);
}

DOMXPathResult *DOMXPath::evaluate(const std::string &xpath,DOMNode node,DOMXPathResult::ResultType result_type)
{
	Token *result = eval.Evaluate(xpath,node);
	
	if(result_type==DOMXPathResult::FIRST_RESULT_TYPE)
	{
		DOMXPathResult *res = new DOMXPathResult(result);
		res->snapshotItem(0);
		
		return res;
	}
	else if(result_type==DOMXPathResult::BOOLEAN_TYPE)
	{
		if(result->GetType()!=LIT_BOOL)
			throw Exception("DOMXPath","Expected boolean result");
		return new DOMXPathResult(result);
	}
	else if(result_type==DOMXPathResult::SNAPSHOT_RESULT_TYPE)
		return new DOMXPathResult(result);
	
	return 0;
}