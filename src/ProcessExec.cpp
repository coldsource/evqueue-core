#include <ProcessExec.h>
#include <Exception.h>
#include <DataSerializer.h>

#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

ProcessExec::ProcessExec()
{
}

ProcessExec::ProcessExec(const string &path)
{
	this->path = path;
}

void ProcessExec::SetPath(const string &path)
{
	this->path = path;
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
		
		if(stdin_pipe[0]!=-1)
		{
			// Send data to the child's stdin
			int written = write(stdin_pipe[1],stdin_data.c_str(),stdin_data.length());
		
			close(stdin_pipe[0]);
			close(stdin_pipe[1]);
			
			if(written!=stdin_data.length())
				throw Exception("ProcessExec","Unable to write parameters to pipe");
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