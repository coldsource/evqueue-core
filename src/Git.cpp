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

#include <Git.h>
#include <LibGit2.h>
#include <SocketQuerySAX2Handler.h>
#include <QueryResponse.h>
#include <XMLUtils.h>
#include <Workflow.h>
#include <Workflows.h>
#include <Task.h>
#include <Tasks.h>
#include <Configuration.h>
#include <Exception.h>
#include <FileManager.h>
#include <Logger.h>
#include <DB.h>
#include <User.h>
#include <base64.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include <string>
#include <memory>

using namespace std;

Git *Git::instance = 0;

Git::Git()
{
	pthread_mutex_init(&lock, NULL);
	
	Configuration *config = Configuration::GetInstance();
	
	repo_path = config->Get("git.repository");
	
	if(repo_path.length())
		repo = new LibGit2(repo_path);
	
	workflows_subdirectory = config->Get("git.workflows.subdirectory");
	tasks_subdirectory = config->Get("git.tasks.subdirectory");
	
	instance = this;
}

Git::~Git()
{
	if(repo)
		delete repo;
}

void Git::SaveWorkflow(const string &name, const string &commit_log, bool force)
{
	pthread_mutex_lock(&lock);
	
	try
	{
		// Get workflow form its ID
		Workflow workflow = Workflows::GetInstance()->Get(name);
		
		// Before doing anything, we have to check last commit on disk is the same as in database (to prevent discarding unseen modifications)
		string db_lastcommit = workflow.GetLastCommit();
		
		// Prepare XML
		string workflow_xml = workflow.SaveToXML();
		
		// Write file to repo
		string commit_id = save_file(workflows_subdirectory+"/"+name+".xml", workflow_xml, db_lastcommit, commit_log, force);
		
		// Update workflow 'lastcommit' to new commit and reload configuration
		workflow.SetLastCommit(commit_id);
		Workflows::GetInstance()->Reload();
	}
	catch(Exception  &e)
	{
		pthread_mutex_unlock(&lock);
		
		throw e;
	}
	
	pthread_mutex_unlock(&lock);
}

void Git::SaveTask(const string &name, const string &commit_log, bool force)
{
	pthread_mutex_lock(&lock);
	
	try
	{
		// Get workflow form its ID
		Task task = Tasks::GetInstance()->Get(name);
		
		// Before doing anything, we have to check last commit on disk is the same as in database (to prevent discarding unseen modifications)
		string db_lastcommit = task.GetLastCommit();
		
		// Prepare XML
		string task_xml = task.SaveToXML();
		
		// Write file to repo
		string commit_id = save_file(tasks_subdirectory+"/"+name+".xml", task_xml, db_lastcommit, commit_log, force);
		
		// Update workflow 'lastcommit' to new commit and reload configuration
		task.SetLastCommit(commit_id);
		Tasks::GetInstance()->Reload();
	}
	catch(Exception  &e)
	{
		pthread_mutex_unlock(&lock);
		
		throw e;
	}
	
	pthread_mutex_unlock(&lock);
}

void Git::LoadWorkflow(const string &name)
{
	pthread_mutex_lock(&lock);
	
	try
	{
		string filename = workflows_subdirectory+"/"+name+".xml";
		
		// Load XML from file
		unique_ptr<DOMDocument> xmldoc(load_file(filename));
		
		// Read repository lastcommit
		string repo_lastcommit = repo->GetFileLastCommit(filename);
		if(!repo_lastcommit.length())
			throw Exception("Git","Unable to get file last commit ID, maybe you need to commit first ?");
		
		// Create/Edit workflow
		Workflow::LoadFromXML(name, xmldoc.get(), repo_lastcommit);
	}
	catch(Exception &e)
	{
		pthread_mutex_unlock(&lock);
		
		throw e;
	}
	
	pthread_mutex_unlock(&lock);
}

void Git::LoadTask(const string &name)
{
	pthread_mutex_lock(&lock);
	
	try
	{
		string filename = tasks_subdirectory+"/"+name+".xml";
		
		// Load XML from file
		unique_ptr<DOMDocument> xmldoc(load_file(filename));
		
		// Read repository lastcommit
		string repo_lastcommit = repo->GetFileLastCommit(filename);
		if(!repo_lastcommit.length())
			throw Exception("Git","Unable to get file last commit ID, maybe you need to commit first ?");
		
		// Create/Edit workflow
		Task::LoadFromXML(name, xmldoc.get(), repo_lastcommit);
	}
	catch(Exception &e)
	{
		pthread_mutex_unlock(&lock);
		
		throw e;
	}
	
	pthread_mutex_unlock(&lock);
}

void Git::GetWorkflow(const string &name, QueryResponse *response)
{
	pthread_mutex_lock(&lock);
	
	try
	{
		string filename = workflows_subdirectory+"/"+name+".xml";
		
		// Load XML from file
		unique_ptr<DOMDocument> xmldoc(load_file(filename));
		
		DOMDocument *response_xmldoc = response->GetDOM();
		DOMNode node = response_xmldoc->importNode(xmldoc->getDocumentElement(),true);
		response_xmldoc->getDocumentElement().appendChild(node);
	}
	catch(Exception &e)
	{
		pthread_mutex_unlock(&lock);
		
		throw e;
	}
	
	pthread_mutex_unlock(&lock);
}

void Git::GetTask(const string &name, QueryResponse *response)
{
	pthread_mutex_lock(&lock);
	
	try
	{
		string filename = tasks_subdirectory+"/"+name+".xml";
		
		// Load XML from file
		unique_ptr<DOMDocument> xmldoc(load_file(filename));
		
		DOMDocument *response_xmldoc = response->GetDOM();
		DOMNode node = response_xmldoc->importNode(xmldoc->getDocumentElement(),true);
		response_xmldoc->getDocumentElement().appendChild(node);
	}
	catch(Exception &e)
	{
		pthread_mutex_unlock(&lock);
		
		throw e;
	}
	
	pthread_mutex_unlock(&lock);
}

string Git::GetWorkflowHash(const string &name)
{
	pthread_mutex_lock(&lock);
	
	string hash = get_file_hash(workflows_subdirectory+"/"+name+".xml");
	
	pthread_mutex_unlock(&lock);
	
	return hash;
	
}

string Git::GetTaskHash(const string &name)
{
	pthread_mutex_lock(&lock);
	
	string hash = get_file_hash(tasks_subdirectory+"/"+name+".xml");
	
	pthread_mutex_unlock(&lock);
	
	return hash;
	
}

void Git::RemoveWorkflow(const std::string &name, const string &commit_log)
{
	pthread_mutex_lock(&lock);
	
	try
	{
		string filename = workflows_subdirectory+"/"+name+".xml";
		string full_filename = repo_path+"/"+filename;
		
		repo->Pull();
		repo->RemoveFile(filename);
		repo->Commit(commit_log);
		repo->Push();
		
		if(unlink(full_filename.c_str())!=0)
			throw Exception("Git","Unable to remove file "+full_filename);
		
		if(Workflows::GetInstance()->Exists(name))
		{
			Workflow workflow = Workflows::GetInstance()->Get(name);
			workflow.SetLastCommit("");
			
			Workflows::GetInstance()->Reload();
		}
	}
	catch(Exception &e)
	{
		pthread_mutex_unlock(&lock);
		
		throw e;
	}
	
	pthread_mutex_unlock(&lock);
}

void Git::RemoveTask(const std::string &name, const string &commit_log)
{
	pthread_mutex_lock(&lock);
	
	try
	{
		string filename = tasks_subdirectory+"/"+name+".xml";
		string full_filename = repo_path+"/"+filename;
		
		repo->Pull();
		repo->RemoveFile(filename);
		repo->Commit(commit_log);
		repo->Push();
		
		if(unlink(full_filename.c_str())!=0)
			throw Exception("Git","Unable to remove file "+full_filename);
		
		if(Tasks::GetInstance()->Exists(name))
		{
			Task task = Tasks::GetInstance()->Get(name);
			task.SetLastCommit("");
			
			Tasks::GetInstance()->Reload();
		}
	}
	catch(Exception &e)
	{
		pthread_mutex_unlock(&lock);
		
		throw e;
	}
	
	pthread_mutex_unlock(&lock);
}

void Git::ListWorkflows(QueryResponse *response)
{
	pthread_mutex_lock(&lock);
	
	try
	{
		list_files(workflows_subdirectory,response);
	}
	catch(Exception &e)
	{
		pthread_mutex_unlock(&lock);
		
		throw e;
	}
	
	pthread_mutex_unlock(&lock);
}

void Git::ListTasks(QueryResponse *response)
{
	pthread_mutex_lock(&lock);
	
	try
	{
		list_files(tasks_subdirectory,response);
	}
	catch(Exception &e)
	{
		pthread_mutex_unlock(&lock);
		
		throw e;
	}
	
	pthread_mutex_unlock(&lock);
}

bool Git::HandleQuery(const User &user, SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	if(!instance->repo)
		throw Exception("Git", "No git repository is configured, this feature is disabled");
	
	const string action = saxh->GetRootAttribute("action");
	
	if(action=="pull")
	{
		Git::GetInstance()->repo->Pull();
		
		return true;
	}
	if(action=="save_workflow")
	{
		string name = saxh->GetRootAttribute("name");
		string commit_log = saxh->GetRootAttribute("commit_log");
		bool force = saxh->GetRootAttributeBool("force",false);
		
		Git::GetInstance()->SaveWorkflow(name,commit_log,force);
		
		return true;
	}
	else if(action=="load_workflow")
	{
		string name = saxh->GetRootAttribute("name");
		
		Git::GetInstance()->LoadWorkflow(name);
		
		return true;
	}
	else if(action=="get_workflow")
	{
		string name = saxh->GetRootAttribute("name");
		
		Git::GetInstance()->GetWorkflow(name,response);
		
		return true;
	}
	else if(action=="remove_workflow")
	{
		string name = saxh->GetRootAttribute("name");
		string commit_log = saxh->GetRootAttribute("commit_log");
		
		Git::GetInstance()->RemoveWorkflow(name,commit_log);
		
		return true;
	}
	else if(action=="list_workflows")
	{
		Git::GetInstance()->ListWorkflows(response);
		return true;
	}
	if(action=="save_task")
	{
		string name = saxh->GetRootAttribute("name");
		string commit_log = saxh->GetRootAttribute("commit_log");
		bool force = saxh->GetRootAttributeBool("force",false);
		
		Git::GetInstance()->SaveTask(name,commit_log,force);
		
		return true;
	}
	else if(action=="load_task")
	{
		string name = saxh->GetRootAttribute("name");
		
		Git::GetInstance()->LoadTask(name);
		
		return true;
	}
	else if(action=="get_task")
	{
		string name = saxh->GetRootAttribute("name");
		
		Git::GetInstance()->GetTask(name,response);
		
		return true;
	}
	else if(action=="remove_task")
	{
		string name = saxh->GetRootAttribute("name");
		string commit_log = saxh->GetRootAttribute("commit_log");
		
		Git::GetInstance()->RemoveTask(name,commit_log);
		
		return true;
	}
	else if(action=="list_tasks")
	{
		Git::GetInstance()->ListTasks(response);
		return true;
	}
	
	return false;
}

string Git::save_file(const string &filename, const string &content, const string &db_lastcommit, const string &commit_log, bool force)
{
	// First we need to pull repository to have a clean workking copy
	repo->Pull();
	
	// Before doing anything, we have to check last commit on disk is the same as in database (to prevent discarding unseen modifications)
	if(!force && db_lastcommit.length())
	{
		string repo_lastcommit = repo->GetFileLastCommit(filename);
		if(db_lastcommit!=repo_lastcommit)
			throw Exception("Git","Database is at a different commit than repository. This could overwrite unwanted data.");
	}
	
	// Compute sha1 on repo before writing
	string hash_before = "";
	try
	{
		FileManager::GetFileHash(repo_path, filename, hash_before);
	}
	catch(Exception &e) {}
	
	// Write file to repo
	string full_filename = repo_path+"/"+filename;
	
	FILE *f = fopen(full_filename.c_str(), "w");
	if(!f)
		throw Exception("Git","Could not open file "+full_filename+" for writing");
	
	if(fwrite(content.c_str(), 1, content.length(), f)!=content.length())
	{
		fclose(f);
		throw Exception("Git","Error writing workflow file");
	}
	
	fclose(f);
	
	// Compute hash after writing
	string hash_after;
	FileManager::GetFileHash(repo_path, filename, hash_after);
	
	// Check we have an effective modification to prevent empty commits
	if(hash_before==hash_after)
		throw Exception("Git","No modifications on file, nothing to commit");
	
	// Commit + Push
	repo->AddFile(filename);
	
	string commit_id;
	try
	{
		commit_id = repo->Commit(commit_log);
	}
	catch(Exception &e)
	{
		repo->ResetLastCommit();
		
		throw e;
	}
	
	repo->Push();
	
	return commit_id;
}

DOMDocument *Git::load_file(const string &filename)
{
	if(repo->StatusIsModified(filename))
		throw Exception("Git", "File has local modifications, discarding");
	
	string full_filename = repo_path+"/"+filename;
	
	// Load XML from file
	DOMDocument *xmldoc = DOMDocument::ParseFile(full_filename);
	if(!xmldoc)
		throw Exception("Git", "Could not parse XML file "+full_filename);
	
	return xmldoc;
}

void Git::list_files(const std::string directory, QueryResponse *response)
{
	string full_directory = repo_path+"/"+directory;
	
	DIR *dh = opendir(full_directory.c_str());
	if(!dh)
		throw Exception("Git","Unable to open directory "+directory);
	
	struct dirent *result;
	
	while(true)
	{
		errno = 0;
		result = readdir(dh);
		if(result==0 && errno!=0)
		{
			closedir(dh);
			throw Exception("Git","Unable to read directory "+directory);
		}
		
		if(result==0)
			break;
		
		if(result->d_name[0]=='.')
			continue; // Skip hidden files
		
		int len = strlen(result->d_name);
		if(len<=4)
			continue;
		
		if(strcasecmp(result->d_name+len-4,".xml")!=0)
			continue;
		
		string entry_name_str(result->d_name,len-4);
		
		DOMElement node = (DOMElement)response->AppendXML("<entry />");
		node.setAttribute("name",entry_name_str);
		
		string lastcommit = repo->GetFileLastCommit(directory+"/"+string(result->d_name));
		node.setAttribute("lastcommit",lastcommit);
	}
	
	closedir(dh);
}

string Git::get_file_hash(const string filename)
{
	string full_filename = repo_path+"/"+filename;
	
	string hash;
	try
	{
		FileManager::GetFileHash(repo_path,filename,hash);
	}
	catch(Exception &e) {}
	
	return hash;
}