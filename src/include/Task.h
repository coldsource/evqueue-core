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

#ifndef _TASK_H_
#define _TASK_H_

#include <DOMElement.h>

#include <string>
#include <vector>

namespace task_parameters_mode { enum task_parameters_mode {ENV,CMDLINE,UNKNOWN}; }
namespace task_output_method { enum task_output_method {XML,TEXT,UNKNOWN}; }

class DB;
class SocketQuerySAX2Handler;
class QueryResponse;
class User;
class DOMDocument;

class Task
{
	std::string path;
	std::vector<std::string> arguments;
	std::string wd;
	bool use_agent;
	task_parameters_mode::task_parameters_mode parameters_mode;
	task_output_method::task_output_method output_method;
	bool merge_stderr;
		
	public:
		Task();
		Task(DOMElement task_node);
		
		const std::string &GetPath() const { return path; }
		const std::vector<std::string> &GetArguments() const { return arguments; }
		const std::string &GetWorkingDirectory() const { return wd; }
		bool GetMergeStderr() const { return  merge_stderr; }
		bool GetUseAgent() const { return  use_agent; }
		bool IsAbsolutePath() const { return path[0]=='/'?true:false; }
		task_parameters_mode::task_parameters_mode GetParametersMode() const  { return parameters_mode; }
		task_output_method::task_output_method GetOutputMethod() const  { return output_method; }
};

#endif
