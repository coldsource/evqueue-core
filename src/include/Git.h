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

#ifndef _GIT_H_
#define _GIT_H_

class SocketQuerySAX2Handler;
class QueryResponse;
class LibGit2;
class User;
class DOMDocument;

#include <mutex>
#include <string>

class Git
{
	static Git *instance;
	
	std::mutex lock;
	
	std::string repo_path;
	
#ifdef USELIBGIT2
	LibGit2 *repo = 0;
#endif
	
	std::string workflows_subdirectory;
	std::string tasks_subdirectory;
	
	public:
		Git();
		~Git();
		
		static Git *GetInstance() { return  instance; }
		
#ifdef USELIBGIT2
		void SaveWorkflow(const std::string &name, const std::string &commit_log, bool force);
		void LoadWorkflow(const std::string &name);
		void GetWorkflow(const std::string &name, QueryResponse *response);
		std::string GetWorkflowHash(const std::string &rev,const std::string &name);
		void RemoveWorkflow(const std::string &name,const std::string &commit_log);
		void ListWorkflows(QueryResponse *response);
#endif
		
		static bool HandleQuery(const User &user, SocketQuerySAX2Handler *saxh, QueryResponse *response);
	
	private:
#ifdef USELIBGIT2
		std::string save_file(const std::string &filename, const std::string &content, const std::string &db_lastcommit, const std::string &commit_log, bool force);
		DOMDocument *load_file(const std::string &filename);
		void list_files(const std::string directory, QueryResponse *response);
		std::string get_file_hash(const std::string filename);
#endif
};

#endif
