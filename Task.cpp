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

#include <string.h>

Task::Task()
{
	task_binary = 0;
	task_wd = 0;
	task_user = 0;
	task_host = 0;
	parameters_mode = task_parameters_mode::UNKNOWN;
	output_method = task_output_method::UNKNOWN;
}

Task::Task(DB *db,const char *task_name)
{
	db->QueryPrintf("SELECT task_binary,task_wd,task_user,task_host,task_parameters_mode,task_output_method FROM t_task WHERE task_name=%s",task_name);
	
	if(!db->FetchRow())
		throw Exception("Task","Unknown task");
	
	task_binary = new char[strlen(db->GetField(0))+1];
	strcpy(task_binary,db->GetField(0));
	
	if(db->GetField(1))
	{
		task_wd = new char[strlen(db->GetField(1))+1];
		strcpy(task_wd,db->GetField(1));
	}
	else
		task_wd = 0;
	
	if(db->GetField(2))
	{
		task_user = new char[strlen(db->GetField(2))+1];
		strcpy(task_user,db->GetField(2));
	}
	else
		task_user = 0;
	
	if(db->GetField(3))
	{
		task_host = new char[strlen(db->GetField(3))+1];
		strcpy(task_host,db->GetField(3));
	}
	else
		task_host = 0;
	
	if(strcmp(db->GetField(4),"ENV")==0)
		parameters_mode = task_parameters_mode::ENV;
	else
		parameters_mode = task_parameters_mode::CMDLINE;
	
	if(strcmp(db->GetField(5),"XML")==0)
		output_method = task_output_method::XML;
	else
		output_method = task_output_method::TEXT;
}

Task::Task(const Task &task)
{
	task_binary = new char[strlen(task.task_binary)+1];
	strcpy(task_binary,task.task_binary);
	
	if(task.task_wd)
	{
		task_wd = new char[strlen(task.task_wd)+1];
		strcpy(task_wd,task.task_wd);
	}
	else
		task_wd = 0;
	
	if(task.task_user)
	{
		task_user = new char[strlen(task.task_user)+1];
		strcpy(task_user,task.task_user);
	}
	else
		task_user = 0;
	
	if(task.task_host)
	{
		task_host = new char[strlen(task.task_host)+1];
		strcpy(task_host,task.task_host);
	}
	else
		task_host = 0;
	
	parameters_mode = task.parameters_mode;
	output_method = task.output_method;
}

Task::~Task()
{
	free();
}

Task &Task::operator=(const Task &task)
{
	free();
	
	task_binary = new char[strlen(task.task_binary)+1];
	strcpy(task_binary,task.task_binary);
	
	if(task.task_wd)
	{
		task_wd = new char[strlen(task.task_wd)+1];
		strcpy(task_wd,task.task_wd);
	}
	
	if(task.task_user)
	{
		task_user = new char[strlen(task.task_user)+1];
		strcpy(task_user,task.task_user);
	}
	
	if(task.task_host)
	{
		task_host = new char[strlen(task.task_host)+1];
		strcpy(task_host,task.task_host);
	}
	
	parameters_mode = task.parameters_mode;
	output_method = task.output_method;
}

void Task::free(void)
{
	if(task_binary)
	{
		delete[] task_binary;
		task_binary = 0;
	}
	
	if(task_wd)
	{
		delete[] task_wd;
		task_wd = 0;
	}
	
	if(task_user)
	{
		delete[] task_user;
		task_user = 0;
	}
	
	if(task_host)
	{
		delete[] task_host;
		task_host = 0;
	}
}
