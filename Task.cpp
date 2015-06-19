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
#include <Configuration.h>

#include <string.h>

using namespace std;

Task::Task()
{
	parameters_mode = task_parameters_mode::UNKNOWN;
	output_method = task_output_method::UNKNOWN;
}

Task::Task(DB *db,const string &task_name)
{
	db->QueryPrintf("SELECT task_binary,task_wd,task_user,task_host,task_parameters_mode,task_output_method FROM t_task WHERE task_name=%s",task_name.c_str());
	
	if(!db->FetchRow())
		throw Exception("Task","Unknown task");
	
	task_binary = db->GetField(0);
		
	if(db->GetField(1))
		task_wd = db->GetField(1);
	
	if(db->GetField(2))
		task_user = db->GetField(2);

	if(db->GetField(3))
		task_host = db->GetField(3);
	
	if(strcmp(db->GetField(4),"ENV")==0)
		parameters_mode = task_parameters_mode::ENV;
	else
		parameters_mode = task_parameters_mode::CMDLINE;
	
	if(strcmp(db->GetField(5),"XML")==0)
		output_method = task_output_method::XML;
	else
		output_method = task_output_method::TEXT;
}

void Task::PutFile(const string &filename,const string &data)
{
	FileManager::PutFile(Configuration::GetInstance()->Get("processmanager.tasks.directory"),filename,data,FileManager::FILETYPE_BINARY);
}

void Task::GetFile(const string &filename,string &data)
{
	FileManager::GetFile(Configuration::GetInstance()->Get("processmanager.tasks.directory"),filename,data);
}

void Task::RemoveFile(const string &filename)
{
	FileManager::RemoveFile(Configuration::GetInstance()->Get("processmanager.tasks.directory"),filename);
}
