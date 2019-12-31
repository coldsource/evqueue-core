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

#include <API/XMLQuery.h>

#include <memory>

using namespace std;

XMLQuery::XMLQuery(const std::string &context, int s):XMLMessage(context,s)
{
}

XMLQuery::XMLQuery(const std::string &context, const std::string &xml):XMLMessage(context,xml)
{
}

XMLQuery::~XMLQuery()
{
	if(parameters)
		delete parameters;
}

string XMLQuery::GetQueryGroup()
{
	return xmldoc->getDocumentElement().getNodeName();
}

WorkflowParameters *XMLQuery::GetWorkflowParameters()
{
	if(parameters)
		return parameters;
	
	parameters = new WorkflowParameters();
	
	unique_ptr<DOMXPathResult> res(xmldoc->evaluate("/*/parameter",xmldoc->getDocumentElement(),DOMXPathResult::SNAPSHOT_RESULT_TYPE));
	
	int i = 0;
	DOMElement node;
	while(res->snapshotItem(i++))
	{
		node = (DOMElement)res->getNodeValue();
		parameters->Add(node.getAttribute("name"),node.getAttribute("value"));
	}
	
	return parameters;
}
