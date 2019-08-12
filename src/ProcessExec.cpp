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

#include <ProcessExec.h>
#include <Exception.h>
#include <DataSerializer.h>

#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

using namespace std;

ProcessExec::ProcessExec()
{
}

ProcessExec::ProcessExec(const string &path)
{
	this->path = path;
}

ProcessExec::~ProcessExec()
{
	for(auto it=file_rdr.begin();it!=file_rdr.end();++it)
		close(it->second);
}

void ProcessExec::SetPath(const string &path)
{
	this->path = path;
}

void ProcessExec::SetScript(const string &directory, const string &script)
{
	pid_t pid = getpid();
	
	string filename = directory+"/"+to_string(pid)+".script";
	
	int fd = open(filename.c_str(),O_CREAT|O_TRUNC|O_RDWR,S_IRWXU);
	if(fd<0)
		throw Exception("ProcessExec","Unable to create temporary script : "+filename);
	
	int written = write(fd,script.c_str(),script.length());
	close(fd);
	
	if(written!=script.length())
		throw Exception("ProcessExec","Unable to write to temporary script : "+filename);
	
	path = filename;
}

void ProcessExec::AddArgument(const string &value, bool escape)
{
	if(escape)
	{
		string escaped_value = "'";
		for(int i=0;i<value.length();i++)
		{
			if(value[i]=='\'')
				escaped_value += "\\'";
			else
				escaped_value += value[i];
		}
		escaped_value += "'";
		
		arguments.push_back(escaped_value);
	}
	else
		arguments.push_back(value);
}

void ProcessExec::AddEnv(const string &name, const string &value)
{
	env[name] = value;
}

void ProcessExec::PipeMap(const map<string,string> &data)
{
	init_stdin_pipe();
	
	stdin_data += DataSerializer::Serialize(data);
}

void ProcessExec::PipeString(const string &data)
{
	init_stdin_pipe();
	
	stdin_data += DataSerializer::Serialize(data);
}

void ProcessExec::Pipe(const string &data)
{
	init_stdin_pipe();
	
	stdin_data += data;
}

void ProcessExec::FileRedirect(int fd, const string &filename)
{
	int fno = open_log_file(filename,fd);
	if(fno<0)
		throw Exception("ProcessExec","Unable to redirect fd "+to_string(fd)+" to file "+filename);
	
	file_rdr.insert(pair<int,int>(fd,fno));
}

void ProcessExec::FDRedirect(int src_fd, int dst_fd)
{
	fd_rdr.insert(pair<int,int>(src_fd,dst_fd));
}

int ProcessExec::ParentRedirect(int fd)
{
	int pipe_fd[2];
	if(pipe(pipe_fd)!=0)
		throw Exception("ProcessExec","Unable to execute task : could not create pipe");
	
	parent_rdr[fd] = {pipe_fd[0], pipe_fd[1]};
	return pipe_fd[0];
}

pid_t ProcessExec::Exec()
{
	pid_t pid = fork();
	
	if(pid<0)
		return pid;
	
	if(pid==0)
	{
		setsid(); // This is used to avoid CTRL+C killing all child processes
		
		if(stdin_pipe[0]!=-1)
		{
			// Redirect STDIN
			dup2(stdin_pipe[0],STDIN_FILENO);
			close(stdin_pipe[1]);
		}
		
		// Do files redirections
		for(auto it = file_rdr.begin();it!=file_rdr.end();++it)
		{
			if(dup2(it->second,it->first)<0)
				return pid;
		}
		
		// Do FD redirections
		for(auto it = fd_rdr.begin();it!=fd_rdr.end();++it)
		{
			if(dup2(it->second,it->first)<0)
				return pid;
		}
		
		// Do parent redirections
		for(auto it = parent_rdr.begin();it!=parent_rdr.end();++it)
		{
			if(dup2(it->second.write_end,it->first)<0)
				return pid;
			
			close(it->second.read_end);
		}
		
		// Prepare ENV
		for(auto it=env.begin();it!=env.end();++it)
			setenv(it->first.c_str(),it->second.c_str(),1);
		
		// Exec child
		const char *args[arguments.size()+2];
		args[0] = path.c_str();
		for(int i=0;i<arguments.size();i++)
			args[i+1] =arguments.at(i).c_str();
		args[arguments.size()+1] = (char *)0;
		
		execv(path.c_str(),(char * const *)args);
		
		fprintf(stderr,"Could not execute process: '%s', execv() got error '%s'",path.c_str(),strerror(errno));
		return pid;
	}
	
	if(pid>0)
	{
		// Close parent redirected pipes (write end)
		for(auto it = parent_rdr.begin();it!=parent_rdr.end();++it)
			close(it->second.write_end);
		
		// Close stdin pipe (read end)
		close(stdin_pipe[0]);
		
		if(stdin_pipe[0]!=-1)
		{
			// Send data to the child's stdin
			// It is possible that the child process dies before reading the whole data so we do not want to check the return value of write
			write(stdin_pipe[1],stdin_data.c_str(),stdin_data.length());
		
			close(stdin_pipe[1]);
		}
	}
	
	return pid;
}

void ProcessExec::init_stdin_pipe()
{
	if(stdin_pipe[0]!=-1)
		return;
	
	if(pipe(stdin_pipe)!=0)
		throw Exception("ProcessExec","Unable to execute task : could not create pipe");
}

int ProcessExec::open_log_file(const string &filename_base, int log_fileno)
{
	string filename;
	if(log_fileno==STDOUT_FILENO)
		filename = filename_base+".stdout";
	else if(log_fileno==STDERR_FILENO)
		filename = filename_base+".stderr";
	else
		filename = filename_base+".log";

	int fno;
	fno=open(filename.c_str(),O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR);

	if(fno==-1)
		throw Exception("ProcessExec","Unable to open task output log file: "+filename);

	return fno;
}
