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

#include <WorkflowInstance/WorkflowInstance.h>
#include <Exception/Exception.h>
#include <WorkflowInstance/ExceptionWorkflowContext.h>
#include <XPath/WorkflowXPathFunctions.h>

#include <memory>

using namespace std;

bool WorkflowInstance::handle_condition(DOMElement node,DOMElement context_node,bool can_wait)
{
	if(!node.hasAttribute("condition"))
		return true;

	ExceptionWorkflowContext ctx(node,"Error evaluating condition");
	
	bool needs_wait = false;
	xmldoc->getXPath()->RegisterFunction("evqWait",{WorkflowXPathFunctions::evqWait,&needs_wait});
	
	try
	{
		unique_ptr<DOMXPathResult> test_expr(xmldoc->evaluate(node.getAttribute("condition"),context_node,DOMXPathResult::BOOLEAN_TYPE));
		
		if(!test_expr->getBooleanValue())
		{
			node.setAttribute("status","SKIPPED");
			node.setAttribute("details","Condition evaluates to false");
			return false;
		}
		
		node.removeAttribute("condition");
		node.setAttribute("details","Condition evaluates to true");
		
		return true;
	}
	catch(Exception &e)
	{
		if(needs_wait)
		{
			if(!can_wait)
				throw;
			
			// evqWait() has thrown exception because it needs to wait
			waiting_nodes.push_back(node);
			node.setAttribute("status","WAITING");
			node.setAttribute("details","Waiting for condition to become true");
			waiting_conditions++;
			update_job_statistics("waiting_conditions",1,node);
			return false;
		}
		else
			throw; // Syntax or dynamic exception (ie real error)
	}
}

bool WorkflowInstance::handle_loop(DOMElement node,DOMElement context_node,vector<DOMElement> &nodes, vector<DOMElement> &contexts)
{
	// Check for loop (must expand them before execution)
	if(!node.hasAttribute("loop"))
	{
		nodes.push_back(node);
		contexts.push_back(context_node);
		return false;
	}

	ExceptionWorkflowContext ctx(node,"Error evaluating loop");
	
	string loop_xpath = node.getAttribute("loop");
	node.removeAttribute("loop");
	
	string iteration_condition;
	if(node.hasAttribute("iteration-condition"))
	{
		iteration_condition = node.getAttribute("iteration-condition");
		node.removeAttribute("iteration-condition");
	}
	
	// This is unchecked user input, try evaluation
	DOMNode matching_node;
	unique_ptr<DOMXPathResult> matching_nodes(xmldoc->evaluate(loop_xpath,context_node,DOMXPathResult::SNAPSHOT_RESULT_TYPE));
	
	int matching_nodes_index = 0;
	while(matching_nodes->snapshotItem(matching_nodes_index++))
	{
		matching_node = matching_nodes->getNodeValue();

		DOMNode node_clone = node.cloneNode(true);
		
		if(iteration_condition!="")
			((DOMElement)node_clone).setAttribute("condition",iteration_condition);
		
		node.getParentNode().appendChild(node_clone);
		
		nodes.push_back(node_clone);
		contexts.push_back(matching_node);
	}

	node.getParentNode().removeChild(node);
	
	return true;
}
