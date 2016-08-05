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
#include <Tasks.h>
#include <DB.h>
#include <Exception.h>
#include <FileManager.h>
#include <Configuration.h>
#include <Workflow.h>
#include <Workflows.h>
#include <SocketQuerySAX2Handler.h>
#include <QueryResponse.h>
#include <base64.h>
#include <global.h>

#include <string.h>

#include <xqilla/xqilla-dom3.hpp>

using namespace std;
using namespace xercesc;

Task::Task()
{
	parameters_mode = task_parameters_mode::UNKNOWN;
	output_method = task_output_method::UNKNOWN;
}

Task::Task(DB *db,const string &task_name)
{
	db->QueryPrintf("SELECT task_id,task_name,task_binary,task_binary_content,task_wd,task_user,task_host,task_use_agent,task_parameters_mode,task_output_method,task_merge_stderr,task_group,task_comment FROM t_task WHERE task_name=%s",task_name.c_str());
	
	if(!db->FetchRow())
		throw Exception("Task","Unknown task");
	
	task_id = db->GetFieldInt(0);
	
	this->task_name = db->GetField(1);
	
	if(db->GetField(2))
		task_binary = db->GetField(2);
	
	if(db->GetField(3))
		task_binary_content = db->GetField(3);
		
	if(db->GetField(4))
		task_wd = db->GetField(4);
	
	if(db->GetField(5))
		task_user = db->GetField(5);

	if(db->GetField(6))
		task_host = db->GetField(6);
	
	task_use_agent = db->GetFieldInt(7);
	
	if(strcmp(db->GetField(8),"ENV")==0)
		parameters_mode = task_parameters_mode::ENV;
	else
		parameters_mode = task_parameters_mode::CMDLINE;
	
	if(strcmp(db->GetField(9),"XML")==0)
		output_method = task_output_method::XML;
	else
		output_method = task_output_method::TEXT;
	
	task_merge_stderr = db->GetFieldInt(10);
	
	task_group = db->GetFieldInt(11);
	
	task_comment = db->GetFieldInt(12);
}

void Task::PutFile(const string &filename,const string &data,bool base64_encoded)
{
	if(base64_encoded)
		FileManager::PutFile(Configuration::GetInstance()->Get("processmanager.tasks.directory"),filename,data,FileManager::FILETYPE_BINARY,FileManager::DATATYPE_BASE64);
	else
		FileManager::PutFile(Configuration::GetInstance()->Get("processmanager.tasks.directory"),filename,data,FileManager::FILETYPE_BINARY,FileManager::DATATYPE_BINARY);
}

void Task::GetFile(const string &filename,string &data)
{
	FileManager::GetFile(Configuration::GetInstance()->Get("processmanager.tasks.directory"),filename,data);
}

void Task::GetFileHash(const string &filename,string &hash)
{
	FileManager::GetFileHash(Configuration::GetInstance()->Get("processmanager.tasks.directory"),filename,hash);
}

void Task::RemoveFile(const string &filename)
{
	FileManager::RemoveFile(Configuration::GetInstance()->Get("processmanager.tasks.directory"),filename);
}

bool Task::CheckTaskName(const string &task_name)
{
	int i,len;
	
	len = task_name.length();
	if(len==0 || len>TASK_NAME_MAX_LEN)
		return false;
	
	for(i=0;i<len;i++)
		if(!isalnum(task_name[i]) && task_name[i]!='_' && task_name[i]!='-')
			return false;
	
	return true;
}

void Task::Get(unsigned int id, QueryResponse *response)
{
	Task task = Tasks::GetInstance()->GetTask(id);
	
	DOMElement *node = (DOMElement *)response->AppendXML("<task />");
	node->setAttribute(X("name"),X(task.GetName().c_str()));
	node->setAttribute(X("binary"),X(task.GetBinary().c_str()));
	
	string base64_binary_content;
	base64_encode_string(task.GetBinaryContent(),base64_binary_content);
	node->setAttribute(X("binary_content"),X(base64_binary_content.c_str()));
	
	node->setAttribute(X("wd"),X(task.GetWorkingDirectory().c_str()));
	node->setAttribute(X("user"),X(task.GetUser().c_str()));
	node->setAttribute(X("host"),X(task.GetHost().c_str()));
	node->setAttribute(X("use_agent"),task.GetUseAgent()?X("1"):X("0"));
	node->setAttribute(X("parameters_mode"),task.GetParametersMode()==task_parameters_mode::ENV?X("ENV"):X("CMDLINE"));
	node->setAttribute(X("output_method"),task.GetOutputMethod()==task_output_method::XML?X("XML"):X("TEXT"));
	node->setAttribute(X("merge_stderr"),task.GetMergeStderr()?X("1"):X("0"));
	node->setAttribute(X("group"),X(task.GetGroup().c_str()));
	node->setAttribute(X("comment"),X(task.GetComment().c_str()));
}

void Task::Create(
	const string &name,
	const string &binary,
	const string &binary_content,
	const string &wd,
	const string &user,
	const string &host,
	bool use_agent,
	const string &parameters_mode,
	const string &output_method,
	bool merge_stderr,
	const string &group,
	const string &comment,
	bool create_workflow,
	std::vector<std::string> inputs
)
{
	create_edit_check(name,binary,binary_content,wd,user,host,use_agent,parameters_mode,output_method,merge_stderr,group,comment);
	
	string real_name;
	
	int imerge_stderr = merge_stderr;
	int iuse_agent = use_agent;
	
	unsigned int workflow_id;
	if(create_workflow)
	{
		real_name = "@"+name;
		
		string workflow_xml = Workflow::CreateSimpleWorkflow(real_name,inputs);
		
		string base64;
		base64_encode_string(workflow_xml,base64);
		
		workflow_id = Workflow::Create(name, base64, group, comment);
	}
	else
		real_name = name;
	
	DB db;
	db.QueryPrintf(
		"INSERT INTO t_task(task_name, task_binary, task_binary_content, task_user, task_host, task_parameters_mode, task_output_method, task_wd, task_group, task_comment,workflow_id, task_use_agent, task_merge_stderr) VALUES(%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%i,%i,%i)",
		real_name.c_str(),
		binary.c_str(),
		binary_content.length()?binary_content.c_str():0,
		user.length()?user.c_str():0,
		host.length()?host.c_str():0,
		parameters_mode.c_str(),
		output_method.c_str(),
		wd.length()?wd.c_str():0,
		group.c_str(),
		comment.c_str(),
		create_workflow?&workflow_id:0,
		&iuse_agent,
		&imerge_stderr
		);
}

void Task::Edit(
	unsigned int id,
	const string &name,
	const string &binary,
	const string &binary_content,
	const string &wd,
	const string &user,
	const string &host,
	bool use_agent,
	const string &parameters_mode,
	const string &output_method,
	bool merge_stderr,
	const string &group,
	const string &comment,
	bool *bound_workflow,
	std::vector<std::string> inputs
)
{
	create_edit_check(name,binary,binary_content,wd,user,host,use_agent,parameters_mode,output_method,merge_stderr,group,comment);
	
	string real_name;
	
	int imerge_stderr = merge_stderr;
	int iuse_agent = use_agent;
	
	DB db;
	db.QueryPrintf("SELECT workflow_id FROM t_task WHERE task_id=%i",&id);
	if(!db.FetchRow())
		throw Exception("Task","Task not found");
	
	if(db.GetFieldInt(0))
	{
		// Task is bound to a workflod
		unsigned int workflow_id = db.GetFieldInt(0);
		
		real_name = "@"+name;
		
		string workflow_xml = Workflow::CreateSimpleWorkflow(real_name,inputs);
		
		string base64;
		base64_encode_string(workflow_xml,base64);
		
		Workflow::Edit(workflow_id,name, base64, group, comment);
		
		*bound_workflow = true;
	}
	else
	{
		real_name = name;
		*bound_workflow = false;
	}
	
	db.QueryPrintf(
		"UPDATE t_task SET task_name=%s, task_binary=%s, task_binary_content=%s, task_user=%s, task_host=%s, task_parameters_mode=%s, task_output_method=%s, task_wd=%s, task_group=%s, task_comment=%s, task_use_agent=%i, task_merge_stderr=%i WHERE task_id=%i",
		real_name.c_str(),
		binary.c_str(),
		binary_content.length()?binary_content.c_str():0,
		user.length()?user.c_str():0,
		host.length()?host.c_str():0,
		parameters_mode.c_str(),
		output_method.c_str(),
		wd.length()?wd.c_str():0,
		group.c_str(),
		comment.c_str(),
		&iuse_agent,
		&imerge_stderr,
		&id
		);
}

void Task::Delete(unsigned int id, bool *workflow_deleted)
{
	DB db;
	
	db.QueryPrintf("SELECT workflow_id FROM t_task WHERE task_id=%i",&id);
	if(!db.FetchRow())
		throw Exception("Task","Task not found");
	
	if(db.GetFieldInt(0))
	{
		// Task is bound to a workflod
		unsigned int workflow_id = db.GetFieldInt(0);
		
		db.QueryPrintf("DELETE FROM t_workflow WHERE workflow_id=%i",&workflow_id);
		
		*workflow_deleted = true;
	}
	else
		*workflow_deleted = false;
	
	db.QueryPrintf("DELETE FROM t_task WHERE task_id=%i",&id);
}

void Task::create_edit_check(
	const string &name,
	const string &binary,
	const string &binary_content,
	const string &wd,
	const string &user,
	const string &host,
	bool use_agent,
	const string &parameters_mode,
	const string &output_method,
	bool merge_stderr,
	const string &group,
	const string &comment
)
{
	if(!CheckTaskName(name))
		throw Exception("Task","Invalid task name");
	
	if(binary.length()==0 && binary_content.length()==0)
		throw Exception("Task","binary and binary_content cannot be both empty");
	
	if(parameters_mode!="CMDLINE" && parameters_mode!="ENV")
		throw Exception("Task","parameters_mode must be 'CMDLINE' or 'ENV'");
	
	if(output_method!="XML" && output_method!="TEXT")
		throw Exception("Task","output_method must be 'XML' or 'TEXT'");
}

bool Task::HandleQuery(SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	const std::map<std::string,std::string> attrs = saxh->GetRootAttributes();
	
	auto it_action = attrs.find("action");
	if(it_action==attrs.end())
		return false;
	
	if(it_action->second=="get")
	{
		auto it_id = attrs.find("id");
		if(it_id==attrs.end())
			throw Exception("Task","Missing 'id' attribute");
		
		unsigned int id;
		try
		{
			id = std::stoi(it_id->second);
		}
		catch(...)
		{
			throw Exception("Task","Invalid ID");
		}
		
		Get(id,response);
		
		return true;
	}
	else if(it_action->second=="create" || it_action->second=="edit")
	{
		auto it_name = attrs.find("name");
		if(it_name==attrs.end())
			throw Exception("Task","Missing 'name' attribute");
		string name = it_name->second;
		
		string binary;
		auto it_binary = attrs.find("binary");
		it_binary==attrs.end()?binary="":binary=it_binary->second;
		
		string binary_content;
		auto it_binary_content = attrs.find("binary_content");
		if(it_binary_content==attrs.end())
			binary_content="";
		else
			base64_decode_string(binary_content,it_binary_content->second);
		
		string wd;
		auto it_wd = attrs.find("wd");
		it_wd==attrs.end()?wd="":wd=it_wd->second;
		
		string user;
		auto it_user = attrs.find("user");
		it_user==attrs.end()?user="":user=it_user->second;
		
		string host;
		auto it_host = attrs.find("host");
		it_host==attrs.end()?host="":host=it_host->second;
		
		bool use_agent;
		auto it_use_agent = attrs.find("use_agent");
		if(it_use_agent==attrs.end())
			use_agent=false;
		else
		{
			if(it_use_agent->second=="1")
				use_agent = true;
			else if(it_use_agent->second=="0")
				use_agent = false;
			else
				throw Exception("Task","use_agent must be '0' or '1'");
		}
		
		auto it_parameters_mode = attrs.find("parameters_mode");
		if(it_parameters_mode==attrs.end())
			throw Exception("Task","Missing 'parameters_mode' attribute");
		string parameters_mode = it_parameters_mode->second;
		
		auto it_output_method = attrs.find("output_method");
		if(it_output_method==attrs.end())
			throw Exception("Task","Missing 'output_method' attribute");
		string output_method = it_output_method->second;
		
		bool merge_stderr;
		auto it_merge_stderr = attrs.find("merge_stderr");
		if(it_merge_stderr==attrs.end())
			merge_stderr=false;
		else
		{
			if(it_merge_stderr->second=="1")
				merge_stderr = true;
			else if(it_merge_stderr->second=="0")
				merge_stderr = false;
			else
				throw Exception("Task","merge_stderr must be '0' or '1'");
		}
		
		string group;
		auto it_group = attrs.find("group");
		it_group==attrs.end()?group="":group=it_group->second;
		
		string comment;
		auto it_comment = attrs.find("comment");
		it_comment==attrs.end()?comment="":comment=it_comment->second;
		
		bool create_workflow;
		auto it_create_workflow = attrs.find("create_workflow");
		if(it_create_workflow==attrs.end())
			create_workflow=false;
		else
		{
			if(it_create_workflow->second=="1")
				create_workflow = true;
			else if(it_create_workflow->second=="0")
				create_workflow = false;
			else
				throw Exception("Task","create_workflow must be '0' or '1'");
		}
		
		if(it_action->second=="create")
			Create(name,binary,binary_content,wd,user,host,use_agent,parameters_mode,output_method,merge_stderr,group,comment,create_workflow,saxh->GetInputs());
		else
		{
			auto it_id = attrs.find("id");
			if(it_id==attrs.end())
				throw Exception("Task","Missing 'id' attribute");
			
			unsigned int id;
			try
			{
				id = std::stoi(it_id->second);
			}
			catch(...)
			{
				throw Exception("Task","Invalid ID");
			}
			
			Edit(id,name,binary,binary_content,wd,user,host,use_agent,parameters_mode,output_method,merge_stderr,group,comment,&create_workflow,saxh->GetInputs());
		}
		
		Tasks::GetInstance()->Reload();
		Tasks::GetInstance()->SyncBinaries();
		
		if(create_workflow)
			Workflows::GetInstance()->Reload();
		
		return true;
	}
	else if(it_action->second=="delete")
	{
		auto it_id = attrs.find("id");
		if(it_id==attrs.end())
			throw Exception("Workflow","Missing 'id' attribute");
		
		unsigned int id;
		try
		{
			id = std::stoi(it_id->second);
		}
		catch(...)
		{
			throw Exception("Task","Invalid ID");
		}
		
		bool workflow_deleted;
		Delete(id,&workflow_deleted);
		
		Tasks::GetInstance()->Reload();
		
		if(workflow_deleted)
			Workflows::GetInstance()->Reload();
		
		return true;
	}
	
	return false;
}