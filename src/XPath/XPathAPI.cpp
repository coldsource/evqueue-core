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

#include <XPath/XPathAPI.h>
#include <XPath/XPathEval.h>
#include <DOM/DOMDocument.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <API/Statistics.h>
#include <Exception/Exception.h>
#include <Logger/Logger.h>
#include <User/User.h>
#include <API/QueryHandlers.h>

#include <string>
#include <map>

#ifdef BUILD_MODULE_EVQUEUE_CORE

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	qh->RegisterHandler("xpath",XPathAPI::HandleQuery);
	return (APIAutoInit *)0;
});

#endif


using namespace std;

bool XPathAPI::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	const string action = query->GetRootAttribute("action");
	
	if(action=="parse")
	{
		string expression = query->GetRootAttribute("expression");
		
		DOMDocument doc;
		XPathEval xpath(&doc);
		
		try
		{
			xpath.Parse(expression);
		}
		catch(Exception &e)
		{
			response->GetDOM()->getDocumentElement().setAttribute("parse-error",e.error);
		}
		
		return true;
	}
	
	return false;
}
