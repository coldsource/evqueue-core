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

#include <Exception/ExceptionWorkflowContext.h>
#include <Exception/ExceptionManager.h>
#include <Exception/Exception.h>
#include <DOM/DOMElement.h>

#include <exception>

using namespace std;

ExceptionWorkflowContext::ExceptionWorkflowContext(DOMNode node,const string &log_message)
{
	this->node = node;
	this->log_message = log_message;
}

ExceptionWorkflowContext::~ExceptionWorkflowContext()
{
	if(uncaught_exception())
	{
		Exception *e = ExceptionManager::GetCurrentException();
		if(e && !ExceptionManager::IsExceptionLogged())
		{
			node.setAttribute("status","ABORTED");
			node.setAttribute("error",log_message+" : "+e->error);
			ExceptionManager::SetExceptionLogged();
		}
	}
}