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

#include "evqueue_agent.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/select.h> 
#include <DataSerializer.h>
#include <Configuration.h>
#include <Exception.h>
#include <ProcessExec.h>

#include <map>
#include <string>

using namespace std;

int main(int argc,char ** argv)
{
	if(argc<2)
	{
		fprintf(stderr,"Usage : evqueue_agent <command> <parameters...>\n");
		return -1;
	}
	
	try
	{
		// Unserialize configuration that was piped on stdin
		map<string,string> config_map;
		if(!DataSerializer::Unserialize(STDIN_FILENO,config_map))
			throw Exception("evqueue_agent","Could not read configuration");
		
		// Load config
		Configuration *config = new Configuration(config_map);
		
		// Unserialize ENV that was piped on stdin
		map<string,string> env_map;
		if(!DataSerializer::Unserialize(STDIN_FILENO,env_map))
			throw Exception("evqueue_monitor","Could not read environment");
		
		// Register ENV
		for(auto it = env_map.begin();it!=env_map.end();++it)
			setenv(it->first.c_str(),it->second.c_str(),true);
		
		// Prepare to fork process
		ProcessExec proc(argv[1]);
		
		// Redirect outputs to multiplex them
		int fd_pipe[MAXFD_FORWARD];
		for(int i=0;i<MAXFD_FORWARD;i++)
			fd_pipe[i] = proc.ParentRedirect(STDOUT_FILENO+i);
		
		// Add arguments
		for(int i=2;i<argc;i++)
			proc.AddArgument(argv[i]);
		
		pid_t pid = proc.Exec();
		if(pid<0)
			throw Exception("evqueue_agent","Unable to execute command '"+string(argv[1])+"', fork() returned error");
		else if(pid==0)
			throw Exception("evqueue_agent","Unable to execute command '"+string(argv[1])+"', execv() returned error");
		
		fd_set rfds;
		
		// Multiplex Data to stdout
		int re;
		while(true)
		{
			int maxfd = -1;
			int set_size = 0;
			FD_ZERO(&rfds);
			for(int i=0;i<MAXFD_FORWARD;i++)
			{
				if(fd_pipe[i]!=-1)
				{
					FD_SET(fd_pipe[i],&rfds);
					if(fd_pipe[i]>maxfd)
						maxfd = fd_pipe[i];
					
					set_size++;
				}
			}
			
			if(set_size==0)
				break;
			
			re = select(maxfd+1,&rfds,0,0,0);
			if(re<=0)
				break;
			
			char buf[4096];
			int read_size;
			int read_pipe_index = -1;
			
			for(int i=0;i<MAXFD_FORWARD;i++)
			{
				if(fd_pipe[i]!=-1 && FD_ISSET(fd_pipe[i], &rfds))
				{
					read_size = read(fd_pipe[i],buf,4096);
					if(read_size==0)
					{
						close(fd_pipe[i]);
						fd_pipe[i] = -1;
						continue;
					}
					
					printf("%02d%09d",STDOUT_FILENO+i,read_size);
					fwrite(buf,1,read_size,stdout);
					fflush(stdout);
				}
			}
		}
		
		int status;
		wait(&status);
		
		if(WIFEXITED(status))
			return WEXITSTATUS(status);
		return -1;
	}
	catch(Exception &e)
	{
		fprintf(stderr,"evqueue_agent: %s\n",e.error.c_str());
		return -1;
	}
}
