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
#include <tools_env.h>

int main(int argc,char ** argv)
{
	if(argc<2)
	{
		fprintf(stderr,"Usage : evqueue_agent <command> <parameters...>\n");
		return -1;
	}
	
	int fd_pipe[MAXFD_FORWARD][2];
	
	for(int i=0;i<MAXFD_FORWARD;i++)
	{
		if(pipe(fd_pipe[i])!=0)
		{
			fprintf(stderr,"evqueue_agent : could not create pipe\n");
			return -1;
		}
	}
	
	pid_t pid = fork();
	
	if(pid==0)
	{
		if(!prepare_env())
		{
			fprintf(stderr,"evqueue_agent: Invalid environment\n");
			return -1;
		}
		
		for(int i=0;i<MAXFD_FORWARD;i++)
		{
			close(fd_pipe[i][0]);
		
			dup2(fd_pipe[i][1],STDOUT_FILENO+i);
			close(fd_pipe[i][1]);
		}
		
		char **child_argv = new char*[argc-1];
		for(int i=0;i<argc-1;i++)
			child_argv[i] = argv[i+1];
			
		execv(child_argv[0],child_argv);
		
		fprintf(stderr,"evqueue_agent: Unable to execute %s\n",child_argv[0]);
		return -1;
	}
	
	for(int i=0;i<MAXFD_FORWARD;i++)
		close(fd_pipe[i][1]);
	
	fd_set rfds;
	
	// Multiplex Data to stdout
	int re;
	while(true)
	{
		int set_size = 0;
		FD_ZERO(&rfds);
		for(int i=0;i<MAXFD_FORWARD;i++)
		{
			if(fd_pipe[i][0]!=-1)
			{
				FD_SET(fd_pipe[i][0],&rfds);
				set_size++;
			}
		}
		
		if(set_size==0)
			break;
		
		re = select(fd_pipe[MAXFD_FORWARD-1][0]+1,&rfds,0,0,0);
		if(re<=0)
			break;
		
		char buf[4096];
		int read_size;
		int read_pipe_index = -1;
		
		for(int i=0;i<MAXFD_FORWARD;i++)
		{
			if(fd_pipe[i][0]!=-1 && FD_ISSET(fd_pipe[i][0], &rfds))
			{
				read_size = read(fd_pipe[i][0],buf,4096);
				if(read_size==0)
				{
					close(fd_pipe[i][0]);
					fd_pipe[i][0] = -1;
					continue;
				}
				
				printf("%02d%09d",STDOUT_FILENO+i,read_size);
				fwrite(buf,1,read_size,stdout);
			}
		}
	}
	
	wait(0);
	
	return 0;
}