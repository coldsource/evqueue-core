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

namespace task_parameters_mode { enum task_parameters_mode {ENV,CMDLINE,UNKNOWN}; }
namespace task_output_method { enum task_output_method {XML,TEXT,UNKNOWN}; }

class DB;

class Task
{
	char *task_binary;
	char *task_wd;
	char *task_user;
	char *task_host;
	task_parameters_mode::task_parameters_mode parameters_mode;
	task_output_method::task_output_method output_method;
		
	public:
		Task();
		Task(DB *db,const char *task_name);
		Task(const Task &task);
		~Task();
		
		Task &operator=(const Task &task);
		
		const char *GetBinary() { return task_binary; }
		const char *GetWorkingDirectory() { return task_wd; }
		const char *GetUser() { return task_user; }
		const char *GetHost() { return task_host; }
		bool IsAbsolutePath() { return task_binary[0]=='/'?true:false; }
		task_parameters_mode::task_parameters_mode GetParametersMode() { return parameters_mode; }
		task_output_method::task_output_method GetOutputMethod() { return output_method; }
	
	private:
		void free(void);
};

#endif
