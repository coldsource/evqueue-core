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
#include <vector>

namespace task_parameters_mode { enum task_parameters_mode {ENV,CMDLINE,UNKNOWN}; }
namespace task_output_method { enum task_output_method {XML,TEXT,UNKNOWN}; }

class DB;
class SocketQuerySAX2Handler;
class QueryResponse;

class Task
{
	unsigned int task_id;
	std::string task_name;
	std::string task_binary;
	std::string task_wd;
	std::string task_user;
	std::string task_host;
	bool task_use_agent;
	task_parameters_mode::task_parameters_mode parameters_mode;
	task_output_method::task_output_method output_method;
	bool task_merge_stderr;
	std::string task_group;
	std::string task_comment;
		
	public:
		Task();
		Task(DB *db,const std::string &task_name);
		
		unsigned int GetID() const { return task_id; }
		const std::string &GetName() const { return task_name; }
		const std::string &GetBinary() const { return task_binary; }
		const std::string &GetWorkingDirectory() const { return task_wd; }
		const std::string &GetUser() const { return task_user; }
		const std::string &GetHost() const { return task_host; }
		bool GetMergeStderr() const { return  task_merge_stderr; }
		bool GetUseAgent() const { return  task_use_agent; }
		bool IsAbsolutePath() const { return task_binary[0]=='/'?true:false; }
		task_parameters_mode::task_parameters_mode GetParametersMode() const  { return parameters_mode; }
		task_output_method::task_output_method GetOutputMethod() const  { return output_method; }
		const std::string &GetGroup() const { return task_group; }
		const std::string &GetComment() const { return task_comment; }
		
		static void PutFile(const std::string &filename,const std::string &data,bool base64_encoded=true);
		static void GetFile(const std::string &filename,std::string &data);
		static void GetFileHash(const std::string &filename,std::string &hash);
		static void RemoveFile(const std::string &filename);
		
		static bool CheckTaskName(const std::string &task_name);
		
		static void Get(unsigned int id, QueryResponse *response);
		
		static void Create(
			const std::string &name,
			const std::string &binary,
			const std::string &binary_content,
			const std::string &wd,
			const std::string &user,
			const std::string &host,
			bool use_agent,
			const std::string &parameters_mode,
			const std::string &output_method,
			bool merge_stderr,
			const std::string &group,
			const std::string &comment,
			bool create_workflow,
			std::vector<std::string> inputs
		);
		
		static void Edit(
			unsigned int id,
			const std::string &name,
			const std::string &binary,
			const std::string &binary_content,
			const std::string &wd,
			const std::string &user,
			const std::string &host,
			bool use_agent,
			const std::string &parameters_mode,
			const std::string &output_method,
			bool merge_stderr,
			const std::string &group,
			const std::string &comment,
			bool *bound_workflow,
			std::vector<std::string> inputs
		);
		
		static void Delete(unsigned int id, bool *workflow_deleted);
		
		static bool HandleQuery(SocketQuerySAX2Handler *saxh, QueryResponse *response);
		
	private:
		static void create_edit_check(
			const std::string &name,
			const std::string &binary,
			const std::string &binary_content,
			const std::string &wd,
			const std::string &user,
			const std::string &host,
			bool use_agent,
			const std::string &parameters_mode,
			const std::string &output_method,
			bool merge_stderr,
			const std::string &group,
			const std::string &comment
		);
};

#endif
