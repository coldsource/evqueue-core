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
#include <ConfigurationEvQueue.h>
#include <Exception.h>
#include <FileManager.h>
#include <Logger.h>
#include <DB.h>
#include <User.h>
#include <Sha1String.h>
#include <XMLFormatter.h>
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
#ifdef USELIBGIT2
	Configuration *config = ConfigurationEvQueue::GetInstance();
	
	repo_path = config->Get("git.repository");
	
	if(repo_path.length())
		repo = new LibGit2(repo_path);
	
	workflows_subdirectory = config->Get("git.workflows.subdirectory");
#endif
	
	instance = this;
}

Git::~Git()
{
#ifdef USELIBGIT2
	if(repo)
		delete repo;
#endif
}

#ifdef USELIBGIT2
void Git::SaveWorkflow(const string &name, const string &commit_log, bool force)
{
	unique_lock<mutex> llock(lock);
	
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

void Git::LoadWorkflow(const string &name)
{
	unique_lock<mutex> llock(lock);
	
	if(!repo)
		throw Exception("Git","Unable to access git repository");
	
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

void Git::GetWorkflow(const string &name, QueryResponse *response)
{
	unique_lock<mutex> llock(lock);
	
	string filename = workflows_subdirectory+"/"+name+".xml";
	
	// Load XML from file
	unique_ptr<DOMDocument> xmldoc(load_file(filename));
	
	DOMDocument *response_xmldoc = response->GetDOM();
	DOMNode node = response_xmldoc->importNode(xmldoc->getDocumentElement(),true);
	response_xmldoc->getDocumentElement().appendChild(node);
}

string Git::GetWorkflowHash(const string &rev, const string &name)
{
	unique_lock<mutex> llock(lock);
	
	if(!repo)
		throw Exception("Git","Unable to access git repository");
	
	string filename = workflows_subdirectory+"/"+name+".xml";
	
	// Ensure uniform format
	XMLFormatter formatter(repo->Cat(rev,filename));
	formatter.Format(false);
	
	return Sha1String(formatter.GetOutput()).GetBinary();
}

void Git::RemoveWorkflow(const std::string &name, const string &commit_log)
{
	unique_lock<mutex> llock(lock);
	
	if(!repo)
		throw Exception("Git","Unable to access git repository");
	
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

void Git::ListWorkflows(QueryResponse *response)
{
	unique_lock<mutex> llock(lock);
	
	list_files(workflows_subdirectory,response);
}

#endif // USELIBGIT2

bool Git::HandleQuery(const User &user, SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
#ifndef USELIBGIT2
	throw Exception("Git", "evQueue was not compiled with git support");
#else
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
	
	return false;
#endif // USELIBGIT2
}

#ifdef USELIBGIT2

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

#endif // USELIBGIT2
