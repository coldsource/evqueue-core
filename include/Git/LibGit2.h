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

#ifdef USELIBGIT2

#ifndef _LIBGIT2_H_
#define _LIBGIT2_H_

#include <git2.h>

#include <string>
#include <vector>

#include <Exception.h>
class LigGit2Exception:public Exception
{
	public:
		LigGit2Exception(const git_error *e):Exception("LigGit",e->message) {};
};

class LibGit2
{
	git_repository *repo = 0;
	git_index *repo_idx = 0;
	git_remote *remote = 0;
	
	public:
		LibGit2(std::string path);
		~LibGit2();
		
		void ReferenceToOID(git_oid *oid, std::string ref);
		std::string OIDToString(git_oid *oid);
		
		void AddFile(std::string path);
		void RemoveFile(std::string path);
		std::string GetFileLastCommit(std::string filename);
		bool StatusIsModified(std::string path);
		
		std::string Commit(std::string log);
		
		void Push();
		void Pull();
		
		void Checkout();
		void ResetLastCommit();
		
		std::string Cat(const std::string &rev, const std::string &path);
	
	private:
		static int credentials_callback(git_cred **cred,const char *url,const char *username_from_url,unsigned int allowed_types,void *payload);
		bool delta_with_parent(git_commit *commit, int i, git_diff_options *opts);
};

#endif

#endif