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

#include <memory>

using namespace std;

void WorkflowInstance::replace_values(DOMElement task,DOMElement context_node)
{
	// Expand dynamic task host if needed
	if(task.hasAttribute("host"))
	{
		ExceptionWorkflowContext ctx(task,"Error computing dynamic host");
		
		string attr_val = task.getAttribute("host");
		string expanded_attr_val = xmldoc->ExpandXPathAttribute(attr_val,context_node);
		task.setAttribute("host",expanded_attr_val);
	}
	
	// Expand dynamic queue host if needed
	if(task.hasAttribute("queue_host"))
	{
		ExceptionWorkflowContext ctx(task,"Error computing dynamic queue host");
		
		string attr_val = task.getAttribute("queue_host");
		string expanded_attr_val = xmldoc->ExpandXPathAttribute(attr_val,context_node);
		task.setAttribute("queue_host",expanded_attr_val);
	}
	
	// Expand dynamic task user if needed
	if(task.hasAttribute("user"))
	{
		ExceptionWorkflowContext ctx(task,"Error computing dynamic user");
		
		string attr_val = task.getAttribute("user");
		std::string expanded_attr_val = xmldoc->ExpandXPathAttribute(attr_val,context_node);
		task.setAttribute("user",expanded_attr_val);
	}
	
	unique_ptr<DOMXPathResult> inputs(xmldoc->evaluate("./input",task,DOMXPathResult::SNAPSHOT_RESULT_TYPE));
	int inputs_index = 0;
	DOMElement input;
	while(inputs->snapshotItem(inputs_index++))
	{
		input = (DOMElement)inputs->getNodeValue();
		replace_value(input,context_node);
	}
	
	unique_ptr<DOMXPathResult> stdin(xmldoc->evaluate("./stdin",task,DOMXPathResult::FIRST_RESULT_TYPE));
	if(stdin->isNode())
		replace_value(stdin->getNodeValue(),context_node);
	
	unique_ptr<DOMXPathResult> script(xmldoc->evaluate("./script",task,DOMXPathResult::FIRST_RESULT_TYPE));
	if(script->isNode())
		replace_value(script->getNodeValue(),context_node);
}

void WorkflowInstance::replace_value(DOMElement input,DOMElement context_node)
{
	DOMElement value;
	int values_index;
	
	{
		ExceptionWorkflowContext ctx(input,"Error computing condition");
		
		if(!handle_condition(input,context_node,false))
			return;
	}
	
	vector<DOMElement> inputs;
	vector<DOMElement> contexts;
	handle_loop(input,context_node,inputs,contexts);
	
	for(int i=0;i<inputs.size();i++)
	{
		DOMElement current_input = inputs.at(i);
		
		{
			ExceptionWorkflowContext ctx(current_input,"Error computing input value");
			
			// Replace <value> nodes by their literal value
			unique_ptr<DOMXPathResult> values(xmldoc->evaluate(".//value",current_input,DOMXPathResult::SNAPSHOT_RESULT_TYPE));
			values_index = 0;
			while(values->snapshotItem(values_index++))
			{
				value = (DOMElement)values->getNodeValue();

				// This is unchecked user input. We have to try evaluation
				unique_ptr<DOMXPathResult> value_nodes(xmldoc->evaluate(value.getAttribute("select"),contexts.at(i),DOMXPathResult::FIRST_RESULT_TYPE));
				
				if(value_nodes->isNode())
					value.getParentNode().replaceChild(xmldoc->createTextNode(value_nodes->getStringValue()),value);
			}
		}
		
		{
			ExceptionWorkflowContext ctx(current_input,"Error computing input values from copy node");
			
			// Replace <copy> nodes by their value
			unique_ptr<DOMXPathResult> values(xmldoc->evaluate(".//copy",current_input,DOMXPathResult::SNAPSHOT_RESULT_TYPE));
			values_index = 0;
			while(values->snapshotItem(values_index++))
			{
				value = (DOMElement)values->getNodeValue();
				string xpath_select = value.getAttribute("select");

				// This is unchecked user input. We have to try evaluation
				unique_ptr<DOMXPathResult> result_nodes(xmldoc->evaluate(xpath_select,contexts.at(i),DOMXPathResult::SNAPSHOT_RESULT_TYPE));
				
				int result_index = 0;
				while(result_nodes->snapshotItem(result_index++))
				{
					DOMNode result_node = result_nodes->getNodeValue();
					value.getParentNode().insertBefore(result_node.cloneNode(true),value);
				}

				value.getParentNode().removeChild(value);
			}
		}
	}
}
