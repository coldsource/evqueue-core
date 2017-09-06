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
#include <User.h>
#include <Git.h>
#include <Sha1String.h>
#include <XMLUtils.h>
#include <XMLFormatter.h>
#include <Logger.h>
#include <base64.h>
#include <global.h>

#include <string.h>

using namespace std;

Task::Task()
{
	parameters_mode = task_parameters_mode::UNKNOWN;
	output_method = task_output_method::UNKNOWN;
}

Task::Task(const string &path)
{
	task_id = 0;
	task_name = "dynamic task";
	task_binary = path;
	task_use_agent = false;
	parameters_mode = task_parameters_mode::CMDLINE;
	output_method = task_output_method::TEXT;
	task_merge_stderr = false;
	bound_workflow_id = 0;
}

Task::Task(DB *db,const string &task_name)
{
	db->QueryPrintf("SELECT task_id,task_name,task_binary,task_wd,task_user,task_host,task_use_agent,task_parameters_mode,task_output_method,task_merge_stderr,task_group,task_comment,task_lastcommit,workflow_id FROM t_task WHERE task_name=%s",&task_name);
	
	if(!db->FetchRow())
		throw Exception("Task","Unknown task");
	
	task_id = db->GetFieldInt(0);
	
	this->task_name = db->GetField(1);
	
	if(!db->GetFieldIsNULL(2))
		task_binary = db->GetField(2);
	
	if(!db->GetFieldIsNULL(3))
		task_wd = db->GetField(3);
	
	if(!db->GetFieldIsNULL(4))
		task_user = db->GetField(4);

	if(!db->GetFieldIsNULL(5))
		task_host = db->GetField(5);
	
	task_use_agent = db->GetFieldInt(6);
	
	if(db->GetField(7)=="ENV")
		parameters_mode = task_parameters_mode::ENV;
	else
		parameters_mode = task_parameters_mode::CMDLINE;
	
	if(db->GetField(8)=="XML")
		output_method = task_output_method::XML;
	else
		output_method = task_output_method::TEXT;
	
	task_merge_stderr = db->GetFieldInt(9);
	
	task_group = db->GetField(10);
	
	task_comment = db->GetField(11);
	
	lastcommit = db->GetField(12);
	
	bound_workflow_id = db->GetFieldIsNULL(13)?0:db->GetFieldInt(13);
}

bool Task::GetIsModified()
{
#ifndef USELIBGIT2
	return false;
#else
	// We are not bound to git, so we cannot compute this
	if(lastcommit=="")
		return false;
	
	// Check SHA1 between repo and current instance
	Git *git = Git::GetInstance();
	
	string repo_hash;
	try {
		repo_hash = git->GetTaskHash(lastcommit,task_name);
	} catch(Exception &e) {
		return true; // Errors can happen on repo change, ignore them
	}
	
	// Ensure uniform format
	XMLFormatter formatter(SaveToXML());
	formatter.Format(false);
	
	string db_hash = Sha1String(formatter.GetOutput()).GetBinary();
	
	return (repo_hash!=db_hash);
#endif
}

void Task::SetLastCommit(const std::string &commit_id)
{
	DB db;
	
	db.QueryPrintf("UPDATE t_task SET task_lastcommit=%s WHERE task_id=%i",commit_id.length()?&commit_id:0,&task_id);
}

string Task::SaveToXML()
{
	DOMDocument xmldoc;
	
	DOMElement node = (DOMElement)XMLUtils::AppendXML(&xmldoc, xmldoc, "<task />");
	node.setAttribute("binary",GetBinary());
	node.setAttribute("wd",GetWorkingDirectory());
	node.setAttribute("user",GetUser());
	node.setAttribute("host",GetHost());
	node.setAttribute("use_agent",GetUseAgent()?"yes":"no");
	node.setAttribute("parameters_mode",GetParametersMode()==task_parameters_mode::ENV?"ENV":"CMDLINE");
	node.setAttribute("output_method",GetOutputMethod()==task_output_method::XML?"XML":"TEXT");
	node.setAttribute("merge_stderr",GetMergeStderr()?"yes":"no");
	node.setAttribute("group",GetGroup());
	node.setAttribute("comment",GetComment());
	node.setAttribute("workflow_bound",bound_workflow_id>0?"yes":"no");
	
	return xmldoc.Serialize(xmldoc.getDocumentElement());
}

void Task::LoadFromXML(string name, DOMDocument *xmldoc, string repo_lastcommit)
{
	DOMElement root_node = xmldoc->getDocumentElement();
		
	string binary = XMLUtils::GetAttribute(root_node, "binary", true);
	string binary_content;
	try
	{
		GetFile(binary, binary_content);
	}
	catch(Exception &e) {}
	string wd = XMLUtils::GetAttribute(root_node, "wd", true);
	string user = XMLUtils::GetAttribute(root_node, "user", true);
	string host = XMLUtils::GetAttribute(root_node, "host", true);
	bool use_agent = XMLUtils::GetAttributeBool(root_node, "use_agent", true);
	string parameters_mode = XMLUtils::GetAttribute(root_node, "parameters_mode", true);
	string output_method = XMLUtils::GetAttribute(root_node, "output_method", true);
	bool merge_stderr = XMLUtils::GetAttributeBool(root_node, "merge_stderr", true);
	string group = XMLUtils::GetAttribute(root_node, "group", true);
	string comment = XMLUtils::GetAttribute(root_node, "comment", true);
	
	try
	{
		bool create_workflow = false;
		
		if(Tasks::GetInstance()->Exists(name))
		{
			// Update
			Logger::Log(LOG_INFO,string("Updating task "+name+" from git").c_str());
			
			Task task = Tasks::GetInstance()->Get(name);
			
			vector<string> inputs;
			Task::Edit(task.GetID(),name,binary,binary_content,wd,user,host,use_agent,parameters_mode,output_method,merge_stderr,group,comment,&create_workflow,inputs);
			task.SetLastCommit(repo_lastcommit);
		}
		else
		{
			// Create
			Logger::Log(LOG_INFO,string("Crerating task "+name+" from git").c_str());
			
			vector<string> inputs;
			Task::Create(name,binary,binary_content,wd,user,host,use_agent,parameters_mode,output_method,merge_stderr,group,comment,create_workflow,inputs,repo_lastcommit);
		}
		
		Tasks::GetInstance()->Reload();
		Tasks::GetInstance()->SyncBinaries();
		
		if(create_workflow)
			Workflows::GetInstance()->Reload();
	}
	catch(Exception &e)
	{
		throw e;
	}
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

void Task::get(const Task &task, QueryResponse *response)
{
	DOMElement node = (DOMElement)response->AppendXML("<task />");
	node.setAttribute("id",to_string(task.GetID()));
	node.setAttribute("name",task.GetName());
	node.setAttribute("binary",task.GetBinary());
	node.setAttribute("wd",task.GetWorkingDirectory());
	node.setAttribute("user",task.GetUser());
	node.setAttribute("host",task.GetHost());
	node.setAttribute("use_agent",task.GetUseAgent()?"1":"0");
	node.setAttribute("parameters_mode",task.GetParametersMode()==task_parameters_mode::ENV?"ENV":"CMDLINE");
	node.setAttribute("output_method",task.GetOutputMethod()==task_output_method::XML?"XML":"TEXT");
	node.setAttribute("merge_stderr",task.GetMergeStderr()?"1":"0");
	node.setAttribute("group",task.GetGroup());
	node.setAttribute("comment",task.GetComment());
}

void Task::GetByID(unsigned int id, QueryResponse *response)
{
	Task task = Tasks::GetInstance()->Get(id);
	return get(task,response);
}

void Task::GetByName(const string &name, QueryResponse *response)
{
	Task task = Tasks::GetInstance()->Get(name);
	return get(task,response);
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
	std::vector<std::string> inputs,
	const std::string &lastcommit,
	QueryResponse *response
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
		"INSERT INTO t_task(task_name, task_binary, task_binary_content, task_user, task_host, task_parameters_mode, task_output_method, task_wd, task_group, task_comment,workflow_id, task_use_agent, task_merge_stderr, task_lastcommit) VALUES(%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%i,%i,%i,%s)",
		&real_name,
		&binary,
		binary_content.length()?&binary_content:0,
		user.length()?&user:0,
		host.length()?&host:0,
		&parameters_mode,
		&output_method,
		wd.length()?&wd:0,
		&group,
		&comment,
		create_workflow?&workflow_id:0,
		&iuse_agent,
		&imerge_stderr,
		lastcommit.length()?&lastcommit:0
		);
	
	unsigned int id = db.InsertID();
	
	if(response)
	{
		if(create_workflow)
			response->GetDOM()->getDocumentElement().setAttribute("workflow-id",to_string(workflow_id));
		response->GetDOM()->getDocumentElement().setAttribute("task-id",to_string(id));
	}
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
	
	Task task = Tasks::GetInstance()->Get(id);
	
	string real_name;
	
	int imerge_stderr = merge_stderr;
	int iuse_agent = use_agent;
	
	DB db;
	if(task.bound_workflow_id>0)
	{
		// Task is bound to a workflod
		real_name = "@"+name;
		
		string workflow_xml = Workflow::CreateSimpleWorkflow(real_name,inputs);
		
		string base64;
		base64_encode_string(workflow_xml,base64);
		
		Workflow::Edit(task.bound_workflow_id,name, base64, group, comment);
		
		*bound_workflow = true;
	}
	else
	{
		real_name = name;
		*bound_workflow = false;
	}
	
	db.QueryPrintf(
		"UPDATE t_task SET task_name=%s, task_binary=%s, task_binary_content=%s, task_user=%s, task_host=%s, task_parameters_mode=%s, task_output_method=%s, task_wd=%s, task_group=%s, task_comment=%s, task_use_agent=%i, task_merge_stderr=%i WHERE task_id=%i",
		&real_name,
		&binary,
		binary_content.length()?&binary_content:0,
		user.length()?&user:0,
		host.length()?&host:0,
		&parameters_mode,
		&output_method,
		wd.length()?&wd:0,
		&group,
		&comment,
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
	
	if(binary.length()==0)
		throw Exception("Task","binary path is invalid");
	
	if(parameters_mode!="CMDLINE" && parameters_mode!="ENV")
		throw Exception("Task","parameters_mode must be 'CMDLINE' or 'ENV'");
	
	if(output_method!="XML" && output_method!="TEXT")
		throw Exception("Task","output_method must be 'XML' or 'TEXT'");
}

bool Task::HandleQuery(const User &user, SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	const string action = saxh->GetRootAttribute("action");
	
	if(action=="get")
	{
		unsigned int id = saxh->GetRootAttributeInt("id");
		
		GetByID(id,response);
		
		return true;
	}
	else if(action=="search")
	{
		string name = saxh->GetRootAttribute("name");
		
		GetByName(name,response);
		
		return true;
	}
	else if(action=="create" || action=="edit")
	{
		string name = saxh->GetRootAttribute("name");
		string binary = saxh->GetRootAttribute("binary");
		string binary_content_base64 = saxh->GetRootAttribute("binary_content","");
		string binary_content;
		if(binary_content_base64.length())
			base64_decode_string(binary_content,binary_content_base64);
		string wd = saxh->GetRootAttribute("wd","");
		string user = saxh->GetRootAttribute("user","");
		string host = saxh->GetRootAttribute("host","");
		bool use_agent = saxh->GetRootAttributeBool("use_agent",false);
		string parameters_mode = saxh->GetRootAttribute("parameters_mode");
		string output_method = saxh->GetRootAttribute("output_method");
		bool merge_stderr = saxh->GetRootAttributeBool("merge_stderr",false);
		string group = saxh->GetRootAttribute("group","");
		string comment = saxh->GetRootAttribute("comment","");
		bool create_workflow = saxh->GetRootAttributeBool("create_workflow",false);
		
		if(action=="create")
			Create(name,binary,binary_content,wd,user,host,use_agent,parameters_mode,output_method,merge_stderr,group,comment,create_workflow,saxh->GetInputs(),"",response);
		else
		{
			unsigned int id = saxh->GetRootAttributeInt("id");
			
			Edit(id,name,binary,binary_content,wd,user,host,use_agent,parameters_mode,output_method,merge_stderr,group,comment,&create_workflow,saxh->GetInputs());
		}
		
		Tasks::GetInstance()->Reload();
		Tasks::GetInstance()->SyncBinaries();
		
		if(create_workflow)
			Workflows::GetInstance()->Reload();
		
		return true;
	}
	else if(action=="delete")
	{
		unsigned int id = saxh->GetRootAttributeInt("id");
		
		bool workflow_deleted;
		Delete(id,&workflow_deleted);
		
		Tasks::GetInstance()->Reload();
		
		if(workflow_deleted)
			Workflows::GetInstance()->Reload();
		
		return true;
	}
	
	return false;
}