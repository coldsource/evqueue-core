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

#include <string>

namespace task_parameters_mode { enum task_parameters_mode {ENV,CMDLINE,UNKNOWN}; }
namespace task_output_method { enum task_output_method {XML,TEXT,UNKNOWN}; }

class DB;

class Task
{
	std::string task_binary;
	std::string task_wd;
	std::string task_user;
	std::string task_host;
	bool task_use_agent;
	task_parameters_mode::task_parameters_mode parameters_mode;
	task_output_method::task_output_method output_method;
	bool task_merge_stderr;
		
	public:
		Task();
		Task(DB *db,const std::string &task_name);
		
		const std::string &GetBinary() const { return task_binary; }
		const std::string &GetWorkingDirectory() const { return task_wd; }
		const std::string &GetUser() const { return task_user; }
		const std::string &GetHost() const { return task_host; }
		bool GetMergeStderr() const { return  task_merge_stderr; }
		bool GetUseAgent() const { return  task_use_agent; }
		bool IsAbsolutePath() const { return task_binary[0]=='/'?true:false; }
		task_parameters_mode::task_parameters_mode GetParametersMode() const  { return parameters_mode; }
		task_output_method::task_output_method GetOutputMethod() const  { return output_method; }
		
		static void PutFile(const std::string &filename,const std::string &data,bool base64_encoded=true);
		static void GetFile(const std::string &filename,std::string &data);
		static void GetFileHash(const std::string &filename,std::string &hash);
		static void RemoveFile(const std::string &filename);
};

#endif
