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

#include <Task.h>
#include <DB.h>
#include <Exception.h>
#include <FileManager.h>
#include <ConfigurationEvQueue.h>
#include <Workflow.h>
#include <Workflows.h>
#include <SocketQuerySAX2Handler.h>
#include <QueryResponse.h>
#include <User.h>
#include <XMLUtils.h>
#include <XMLFormatter.h>
#include <Logger.h>
#include <global.h>

#include <string.h>
#include <wordexp.h>

using namespace std;

Task::Task()
{
	parameters_mode = task_parameters_mode::UNKNOWN;
	output_method = task_output_method::UNKNOWN;
}

Task::Task(DOMElement task_node)
{
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
	
	merge_stderr = false;
	if(task_node.hasAttribute("merge-stderr"))
		merge_stderr = XMLUtils::GetAttributeBool(task_node,"merge-stderr");
	
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
