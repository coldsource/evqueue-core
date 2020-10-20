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

#include <WorkflowInstance/Task.h>
#include <DB/DB.h>
#include <Exception/Exception.h>
#include <IO/FileManager.h>
#include <Configuration/ConfigurationEvQueue.h>
#include <Workflow/Workflow.h>
#include <Workflow/Workflows.h>
#include <API/QueryResponse.h>
#include <User/User.h>
#include <XML/XMLUtils.h>
#include <XML/XMLFormatter.h>
#include <DOM/DOMDocument.h>
#include <Logger/Logger.h>
#include <global.h>

#include <string.h>
#include <wordexp.h>

#include <memory>

using namespace std;

Task::Task()
{
	parameters_mode = task_parameters_mode::UNKNOWN;
	output_method = task_output_method::UNKNOWN;
}

Task::Task(DOMDocument *xmldoc, DOMElement task_node)
{
	this->xmldoc = xmldoc;
	this->task_node = task_node;
	
	path = task_node.getAttribute("path");
	
	wd = "";
	if(task_node.hasAttribute("wd"))
		wd = task_node.getAttribute("wd");
	
	use_agent = false;
	if(task_node.hasAttribute("use-agent"))
		use_agent = XMLUtils::GetAttributeBool(task_node,"use-agent");
	
	parameters_mode = task_parameters_mode::CMDLINE;
	if(task_node.hasAttribute("parameters-mode"))
	{
		string parameters_mode_str = task_node.getAttribute("parameters-mode");
		if(parameters_mode_str=="CMDLINE")
			parameters_mode = task_parameters_mode::CMDLINE;
		else if(parameters_mode_str=="ENV")
			parameters_mode = task_parameters_mode::ENV;
		else
			throw Exception("Task","Unkown parameters mode "+parameters_mode_str);
	}
	
	output_method = task_output_method::TEXT;
	if(task_node.hasAttribute("output-method"))
	{
		string output_method_str = task_node.getAttribute("output-method");
		if(output_method_str=="TEXT")
			output_method = task_output_method::TEXT;
		else if(output_method_str=="XML")
			output_method = task_output_method::XML;
		else
			throw Exception("Task","Unkown output method "+output_method_str);
	}
	
	type = task_type::BINARY;
	if(task_node.hasAttribute("type"))
	{
		string type_str = task_node.getAttribute("type");
		if(type_str=="BINARY")
			type = task_type::BINARY;
		else if(type_str=="SCRIPT")
			type = task_type::SCRIPT;
		else
			throw Exception("Task","Unkown type "+type_str);
	}
	
	merge_stderr = false;
	if(task_node.hasAttribute("merge-stderr"))
		merge_stderr = XMLUtils::GetAttributeBool(task_node,"merge-stderr");
	
	if(type==task_type::BINARY)
	{
		// Perform arguments expension
		wordexp_t wexp;
		if(wordexp(path.c_str(), &wexp, 0)!=0)
			throw Exception("Task","Error expanding task command line");
		
		char **w = wexp.we_wordv;
		path = w[0];
		for (int i = 1; i < wexp.we_wordc; i++)
			arguments.push_back(string(w[i]));
		
		wordfree(&wexp);
	}
}

void Task::GetParameters(vector<string> &names, vector<string> &values) const
{
	unique_ptr<DOMXPathResult> parameters(xmldoc->evaluate("input",task_node,DOMXPathResult::SNAPSHOT_RESULT_TYPE));
	DOMElement parameter;
	int parameters_index = 0;

	while(parameters->snapshotItem(parameters_index++))
	{
		parameter = (DOMElement)parameters->getNodeValue();
		
		if(parameter.hasAttribute("status") && parameter.getAttribute("status")=="SKIPPED")
			continue;
		
		if(parameter.hasAttribute("name"))
			names.push_back(parameter.getAttribute("name"));
		else
			names.push_back("");

		values.push_back(parameter.getTextContent());
	}
}

string Task::GetUser() const
{
	string user;
	if(task_node.hasAttribute("host"))
	{
		if(task_node.hasAttribute("user"))
			user = task_node.getAttribute("user");
	}
	else if(xmldoc->getDocumentElement().hasAttribute("host"))
	{
		if(xmldoc->getDocumentElement().hasAttribute("user"))
			user = xmldoc->getDocumentElement().getAttribute("user");
	}
	return user;
}

string Task::GetHost() const
{
	string host;
	if(task_node.hasAttribute("host"))
		host = task_node.getAttribute("host");
	else if(xmldoc->getDocumentElement().hasAttribute("host"))
		host = xmldoc->getDocumentElement().getAttribute("host");
	return host;
}


string Task::GetTypeStr() const
{
	if(type==task_type::BINARY)
		return "BINARY";
	else if(type==task_type::SCRIPT)
		return "SCRIPT";
	throw Exception("Task","Unknown task type");
}

string Task::GetScript() const
{
	string script;
	{
		unique_ptr<DOMXPathResult> parameters(xmldoc->evaluate("script",task_node,DOMXPathResult::FIRST_RESULT_TYPE));
		
		DOMElement script_node = (DOMElement)parameters->getNodeValue();
		if(parameters->isNode())
			script = script_node.getTextContent();
	}
	return script;
}

string Task::GetStdin() const
{
	string stdin_parameter;
	{
		unique_ptr<DOMXPathResult> parameters(xmldoc->evaluate("stdin",task_node,DOMXPathResult::FIRST_RESULT_TYPE));

		if(parameters->isNode())
		{
			DOMElement stdin_node = (DOMElement)parameters->getNodeValue();
			if(stdin_node.hasAttribute("mode") && stdin_node.getAttribute("mode")=="text")
				stdin_parameter = stdin_node.getTextContent();
			else
				stdin_parameter = xmldoc->Serialize(stdin_node);
		}
	}
	return stdin_parameter;
}
