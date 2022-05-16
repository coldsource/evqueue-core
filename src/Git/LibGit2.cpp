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

#include <Git/LibGit2.h>
#include <Configuration/Configuration.h>

#include <string.h>
#include <time.h>

using namespace std;

namespace Git
{

LibGit2::LibGit2(string path)
{
	if(git_repository_open(&repo,path.c_str())!=0)
		throw LigGit2Exception(giterr_last());
	
	if(git_repository_index(&repo_idx, repo)!=0)
		throw LigGit2Exception(giterr_last());
	
	if(git_remote_lookup(&remote, repo, "origin")!=0)
		throw LigGit2Exception(giterr_last());
}

LibGit2::~LibGit2()
{
	if(repo)
		git_repository_free(repo);
	
	if(repo_idx)
		git_index_free(repo_idx);
	
	if(remote)
		git_remote_free(remote);
}

void LibGit2::ReferenceToOID(git_oid *oid, std::string ref)
{
	if(git_reference_name_to_id(oid, repo, ref.c_str())!=0)
		throw LigGit2Exception(giterr_last());
}

std::string LibGit2::OIDToString(git_oid *oid)
{
	char oid_c[GIT_OID_HEXSZ+1];
	oid_c[GIT_OID_HEXSZ] = '\0';
	git_oid_fmt(oid_c, oid);
	
	return string(oid_c);
}

void LibGit2::AddFile(std::string path)
{
	// Add file to memory index
	if(git_index_add_bypath(repo_idx, path.c_str())!=0)
		throw LigGit2Exception(giterr_last());
	
	// Write index to disk
	if(git_index_write(repo_idx)!=0)
		throw LigGit2Exception(giterr_last());
}

void LibGit2::RemoveFile(std::string path)
{
	// Check file exists in index
	const git_index_entry *entry = git_index_get_bypath(repo_idx,path.c_str(), 0);
	if(entry==0)
		throw Exception("LibGit2","File is not in repository");
	
	// Remove file from memory index
	if(git_index_remove_bypath(repo_idx, path.c_str())!=0)
		throw LigGit2Exception(giterr_last());
	
	// Write index to disk
	if(git_index_write(repo_idx)!=0)
		throw LigGit2Exception(giterr_last());
}

string LibGit2::GetFileLastCommit(string filename)
{
	// Prepare the pathspec to filter on the specified filename
	git_pathspec *ps = 0;
	char *filename_c = new char[filename.length()+1];
	strcpy(filename_c, filename.c_str());
	git_strarray ps_array = {&filename_c ,1};
	git_pathspec_new(&ps, &ps_array);
	
	git_diff_options diffopts = GIT_DIFF_OPTIONS_INIT;
	diffopts.pathspec = ps_array;
	
	// Walk revisions list of the repo and find the last revisions having modifications on the specified file
	git_revwalk *revwalk = 0;
	git_oid rev_oid;
	
	git_commit *commit;
	string commit_id_str;
	try
	{
		if(git_revwalk_new(&revwalk,repo)!=0)
			throw LigGit2Exception(giterr_last());
		
		// Sort revisions by time (last revision will be get first)
		git_revwalk_sorting(revwalk,GIT_SORT_TIME);
		
		// Start from HEAD
		if(git_revwalk_push_head(revwalk)!=0)
			throw LigGit2Exception(giterr_last());
		
		// Iterate all revisions
		while(git_revwalk_next(&rev_oid,revwalk)==0)
		{
			if(git_commit_lookup(&commit, repo, &rev_oid)!=0)
				throw LigGit2Exception(giterr_last());
			
			int parents = (int)git_commit_parentcount(commit);
			
			if(parents==0)
			{
				git_tree *commit_tree = 0;
				if(git_commit_tree(&commit_tree, commit)!=0)
					throw LigGit2Exception(giterr_last());
				
				int re = git_pathspec_match_tree(0, commit_tree, GIT_PATHSPEC_NO_MATCH_ERROR, ps);
				
				git_commit_free(commit);
				git_tree_free(commit_tree);
				commit = 0;
				
				if(re!=0)
					continue;
			}
			else if(parents==1)
			{
				bool re = delta_with_parent(commit, 0, &diffopts);
				
				git_commit_free(commit);
				commit = 0;
				
				if(!re)
					continue;
			}
			else
			{
				int ndeltas = 0;
				for(int i=0;i<parents;i++)
					if(delta_with_parent(commit, i, &diffopts))
						ndeltas++;
				
				git_commit_free(commit);
				commit = 0;
				
				if(ndeltas!=parents)
					continue;
			}
			
			commit_id_str = OIDToString(&rev_oid);
			break;
		}
	}
	catch(LigGit2Exception &e)
	{
		delete[] filename_c;
		
		if(revwalk)
			git_revwalk_free(revwalk);
		
		if(ps)
			git_pathspec_free(ps);
		
		if(commit)
			git_commit_free(commit);
		
		throw e;
	}
	
	delete[] filename_c;
	git_revwalk_free(revwalk);
	git_pathspec_free(ps);
	git_commit_free(commit);
	
	return commit_id_str;
}

bool LibGit2::StatusIsModified(string path)
{
	unsigned int status;
	if(git_status_file(&status,repo,path.c_str())!=0)
		throw LigGit2Exception(giterr_last());
	
	return status!=GIT_STATUS_CURRENT;
}


string LibGit2::Commit(std::string log)
{
	git_commit *parent = 0;
	git_tree *tree = 0;
	git_signature *signature;
	git_oid new_commit_id;
	
	try
	{
		// Write index tree to disk and return root OID
		git_oid tree_oid;
		if(git_index_write_tree(&tree_oid, repo_idx)!=0)
			throw LigGit2Exception(giterr_last());
		
		// Lookup written tree from OID
		if(git_tree_lookup(&tree, repo, &tree_oid)!=0)
			throw LigGit2Exception(giterr_last());
		
		// Get current HEAD
		git_oid parent_id;
		ReferenceToOID(&parent_id,"HEAD");
		
		if(git_commit_lookup(&parent, repo, &parent_id)!=0)
			throw LigGit2Exception(giterr_last());
		
		time_t now = time(0);
		struct tm now_tm;
		localtime_r(&now, &now_tm);
		time_t now2 = timegm(&now_tm);
		
		const string signature_name = Configuration::GetInstance()->Get("git.signature.name");
		const string signature_email = Configuration::GetInstance()->Get("git.signature.email");
		if(git_signature_new((git_signature **)&signature, signature_name.c_str(), signature_email.c_str(), now, (now2-now)/60)!=0)
			throw LigGit2Exception(giterr_last());
		
		// Commit changes to HEAD
		int re = git_commit_create_v(
			&new_commit_id,
			repo,
			"HEAD",
			signature,
			signature,
			"UTF-8",
			log.c_str(),
			tree,
			1,
			parent);
		
		if(re!=0)
			throw LigGit2Exception(giterr_last());
	}
	catch(LigGit2Exception &e)
	{
		if(parent)
			git_commit_free(parent);
		
		if(tree)
			git_tree_free(tree);
		
		if(signature)
			git_signature_free(signature);
		
		throw e;
	}
	
	git_commit_free(parent);
	git_tree_free(tree);
	git_signature_free(signature);
	
	return OIDToString(&new_commit_id);
}

void LibGit2::Push()
{
	char *ref_str = new char[18];
	strcpy(ref_str,"refs/heads/master");
	const git_strarray refs = {&ref_str, 1};
	
	git_remote_callbacks *remote_callbacks;
	
	git_push_options push_options;
	git_push_init_options(&push_options,GIT_PUSH_OPTIONS_VERSION);
	git_remote_init_callbacks(&push_options.callbacks,GIT_REMOTE_CALLBACKS_VERSION);
	push_options.callbacks.credentials = LibGit2::credentials_callback;
	
	try
	{
		if(git_remote_push(remote,&refs,&push_options)!=0)
			throw LigGit2Exception(giterr_last());
	}
	catch(Exception &e)
	{
		delete[] ref_str;
		
		throw e;
	}
	
	delete[] ref_str;
}

void LibGit2::Pull()
{
	git_commit *their_commit = 0, *our_commit = 0;
	git_reference *heads_master = 0,*heads_master_new = 0;
	git_index *dst_idx = 0;
	
	try
	{
		// Fetch repository from remote
		git_fetch_options fetch_opts;
		fetch_opts = GIT_FETCH_OPTIONS_INIT;
		git_remote_init_callbacks(&fetch_opts.callbacks,GIT_REMOTE_CALLBACKS_VERSION);
		fetch_opts.callbacks.credentials = LibGit2::credentials_callback;
		if(git_remote_fetch(remote,0,&fetch_opts,"fetch")!=0)
			throw LigGit2Exception(giterr_last());
		
		// Prepare merge : we need their HEAD commit and our HEAD commit to merge
		git_oid their_head;
		ReferenceToOID(&their_head,"FETCH_HEAD");
		
		if(git_commit_lookup(&their_commit, repo, &their_head)!=0)
			throw LigGit2Exception(giterr_last());
		
		git_oid our_head;
		ReferenceToOID(&our_head, "HEAD");
		
		if(git_commit_lookup(&our_commit, repo, &our_head)!=0)
			throw LigGit2Exception(giterr_last());
		
		// Merge remote and local heads
		if(git_merge_commits(&dst_idx,repo, our_commit, their_commit, 0)!=0)
			throw LigGit2Exception(giterr_last());
		
		git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
		checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
		if(git_checkout_index(repo, dst_idx, &checkout_opts)!=0)
			throw LigGit2Exception(giterr_last());
		
		// Update HEAD to new position (fast forword)
		if(git_reference_lookup(&heads_master,repo,"refs/heads/master")!=0)
			throw LigGit2Exception(giterr_last());
		
		if(git_reference_set_target(&heads_master_new,heads_master,&their_head,"")!=0)
			throw LigGit2Exception(giterr_last());
	}
	catch(LigGit2Exception &e)
	{
		if(dst_idx)
			git_index_free(dst_idx);
		
		if(their_commit)
			git_commit_free(their_commit);
		
		if(our_commit)
			git_commit_free(our_commit);
		
		if(heads_master)
			git_reference_free(heads_master);
		
		if(heads_master_new)
			git_reference_free(heads_master_new);
		
		throw e;
	}
	
	git_index_free(dst_idx);
	git_commit_free(their_commit);
	git_commit_free(our_commit);
	git_reference_free(heads_master);
	git_reference_free(heads_master_new);
	
	return;
}

void LibGit2::Checkout()
{
	git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
	checkout_opts.checkout_strategy = GIT_CHECKOUT_FORCE;
	
	if(!git_checkout_head(repo, &checkout_opts))
		throw LigGit2Exception(giterr_last());
}

void LibGit2::ResetLastCommit()
{
	git_oid head_oid;
	ReferenceToOID(&head_oid,"HEAD");
	
	git_oid fetch_head_oid;
	ReferenceToOID(&fetch_head_oid,"FETCH_HEAD");
	
	if(git_oid_cmp(&head_oid, &fetch_head_oid)==0)
		return; // We won't reset before FETCH_HEAD
	
	git_commit *commit = 0, *parent_commmit = 0;
	try
	{
		if(git_commit_lookup(&commit, repo, &head_oid)!=0)
			throw LigGit2Exception(giterr_last());
		
		if(git_commit_parent(&parent_commmit, commit, 0)!=0)
			throw LigGit2Exception(giterr_last());
		
		if(git_reset(repo, (git_object *)parent_commmit, GIT_RESET_HARD, 0)!=0)
			throw LigGit2Exception(giterr_last());
	}
	catch(LigGit2Exception &e)
	{
		if(commit)
			git_commit_free(commit);
		
		if(parent_commmit)
			git_commit_free(parent_commmit);
		
		throw e;
	}
	
	git_commit_free(commit);
	git_commit_free(parent_commmit);
}

string LibGit2::Cat(const string &rev, const string &path)
{
	git_object *obj = NULL;
	string rev_str = rev+":"+path;
	if(git_revparse_single(&obj, repo, rev_str.c_str())!=0)
		throw LigGit2Exception(giterr_last());
	
	const git_blob *blob = (const git_blob *)obj;
	string content((char *)git_blob_rawcontent(blob),git_blob_rawsize(blob));
	git_object_free(obj);
	return content;
}

int LibGit2::credentials_callback(git_cred **cred,const char *url,const char *username_from_url,unsigned int allowed_types,void *payload)
{
	string user = Configuration::GetInstance()->Get("git.user");
	string password = Configuration::GetInstance()->Get("git.password");
	string public_key = Configuration::GetInstance()->Get("git.public_key");
	string private_key = Configuration::GetInstance()->Get("git.private_key");
	
	if(allowed_types & git_credtype_t::GIT_CREDTYPE_SSH_KEY)
		return git_cred_ssh_key_new(cred,user.c_str(),public_key.c_str(),private_key.c_str(),"");
	else if(allowed_types & git_credtype_t::GIT_CREDTYPE_USERPASS_PLAINTEXT)
		return git_cred_userpass_plaintext_new(cred,user.c_str(),password.c_str());
	else
		return GIT_PASSTHROUGH;
}

bool LibGit2::delta_with_parent(git_commit *commit, int i, git_diff_options *opts)
{
	// Check if the specified file has a delta between the commit and it's parent
	git_commit *parent_commit = 0;
	git_tree *parent_tree = 0, *commit_tree = 0;
	git_diff *diff = 0;
	int ndeltas;
	
	try
	{
	  if(git_commit_parent(&parent_commit, commit, (size_t)i)!=0)
		throw LigGit2Exception(giterr_last());
	
	if(git_commit_tree(&parent_tree, parent_commit)!=0)
		throw LigGit2Exception(giterr_last());
	
	if(git_commit_tree(&commit_tree, commit)!=0)
		throw LigGit2Exception(giterr_last());
	
	if(git_diff_tree_to_tree(&diff, git_commit_owner(commit), parent_tree, commit_tree, opts)!=0)
		throw LigGit2Exception(giterr_last());
	}
	catch(LigGit2Exception &e)
	{
		if(parent_tree)
			git_tree_free(parent_tree);
		
		if(commit_tree)
			git_tree_free(commit_tree);
		
		if(parent_commit)
			git_commit_free(parent_commit);
		
		if(diff)
			git_diff_free(diff);
		
		throw e;
	}
	
	ndeltas = (int)git_diff_num_deltas(diff);
	
	git_diff_free(diff);
	git_tree_free(parent_tree);
	git_tree_free(commit_tree);
	git_commit_free(parent_commit);
	
	return ndeltas > 0;
}

}
