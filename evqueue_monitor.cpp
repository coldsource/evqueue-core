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

#include <global.h>
#include <tools_ipc.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>

pid_t pid = 0;

void signal_callback_handler(int signum)
{
	if(signum==SIGTERM)
	{
		// Forward signal to our child
		fprintf(stderr,"evqueue_monitor : received SIGTERM, killing task...\n");
		if(pid)
			kill(pid,SIGKILL);
	}
}

int main(int argc,char ** argv)
{
	if(argc<=2)
	{
		fprintf(stderr,"evqueue_monitor should only be called by evqueue engine\n");
		return -1;
	}
	
	// Catch signals
	signal(SIGTERM,signal_callback_handler);
	
	// Unblock SIGTERM (blocked by evqueue)
	sigset_t signal_mask;
	sigemptyset(&signal_mask);
	sigaddset(&signal_mask, SIGTERM);
	sigprocmask(SIG_UNBLOCK,&signal_mask,0);
	
	// Create message queue
	int msgqid = ipc_openq(getenv("EVQUEUE_IPC_QID"));
	if(msgqid==-1)
		return -1;
	
	pid_t tid = atoi(argv[2]);
	int status;
	
	pid = fork();
	
	if(pid==0)
	{
		const char *working_directory = getenv("EVQUEUE_WORKING_DIRECTORY");
		
		// Compute the number of SSH arguments
		int ssh_nargs = 0;
		if(getenv("EVQUEUE_SSH_HOST"))
			ssh_nargs++;
		if(getenv("EVQUEUE_SSH_KEY"))
			ssh_nargs+=2;
		if(ssh_nargs>0 && working_directory)
			ssh_nargs++;
		
		if(ssh_nargs>0)
			ssh_nargs++; // Add the task binary to the SSH arguments
		
		int task_nargs = argc-3;
		
		// Build task arguments
		int current_arg = 1;
		char **task_argv = new char*[1+ssh_nargs+task_nargs+1];
		task_argv[0] = argv[1];
		
		if(ssh_nargs>0)
		{
			// We are using SSH
			
			// SSH Path
			const char *ssh_path = getenv("EVQUEUE_SSH_PATH");
			task_argv[0] = new char[strlen(ssh_path)+1];
			strcpy(task_argv[0],ssh_path);
			
			// SSH key
			const char *ssh_key = getenv("EVQUEUE_SSH_KEY");
			if(ssh_key)
			{
				task_argv[current_arg] = new char[3];
				strcpy(task_argv[current_arg],"-i");
				current_arg++;
				
				task_argv[current_arg] = new char[strlen(ssh_key)+1];
				strcpy(task_argv[current_arg],ssh_key);
				current_arg++;
			}
			
			// SSH user/host
			const char *ssh_host = getenv("EVQUEUE_SSH_HOST");
			const char *ssh_user = getenv("EVQUEUE_SSH_USER");
			if(ssh_user)
			{
				task_argv[current_arg] = new char[strlen(ssh_host)+strlen(ssh_user)+2];
				sprintf(task_argv[current_arg],"%s@%s",ssh_user,ssh_host);
			}
			else
			{
				task_argv[current_arg] = new char[strlen(ssh_host)+1];
				strcpy(task_argv[current_arg],ssh_host);
			}
			
			current_arg++;
			
			// Working directory
			if(working_directory)
			{
				task_argv[current_arg] = new char[strlen(working_directory)+32];
				sprintf(task_argv[current_arg],"cd %s;",working_directory);
				current_arg++;
			}
			
			// Task binary
			task_argv[current_arg++] = argv[1];
		}
		
		// Add task parameters
		for(int i=0;i<task_nargs;i++)
		{
			if(ssh_nargs>0)
			{
				// Escape arguments
				int len = strlen(argv[i+3]);
				task_argv[current_arg] = new char[2*len+3];
				
				char *ptr = task_argv[current_arg];
				ptr[0] = '\'';
				ptr++;
				for(int j=0;j<len;j++)
				{
					if(argv[i+3][j]=='\'')
					{
						ptr[0] = '\\';
						ptr[1] = '\'';
						ptr+=2;
					}
					else
					{
						ptr[0] = argv[i+3][j];
						ptr++;
					}
				}
				ptr[0] = '\'';
				ptr[1] = '\0';
				
				printf("%s\n",task_argv[current_arg]);
				
				current_arg++;
			}
			else
				task_argv[current_arg++] = argv[i+3];
		}
		
		task_argv[current_arg] = 0;
		
		if(working_directory && ssh_nargs==0)
			chdir(working_directory);
		
		status = execv(task_argv[0],task_argv);
		
		fprintf(stderr,"Unable to execute command '%s'. execv() returned %d\n",argv[1],status);
		return -1;
	}
	
	st_msgbuf msgbuf;
	msgbuf.type = 1;
	msgbuf.mtext.pid = getpid();
	msgbuf.mtext.tid = tid;
	
	if(pid<0)
	{
		fprintf(stderr,"Unable to execute fork\n");
		
		msgbuf.mtext.retcode = -1;
	}
	else
	{
		waitpid(pid,&status,0);
		if(WIFEXITED(status))
			msgbuf.mtext.retcode = WEXITSTATUS(status);
		else
			msgbuf.mtext.retcode = -1;
	}
	
	msgsnd(msgqid,&msgbuf,sizeof(st_msgbuf::mtext),0); // Notify evqueue
	
	return msgbuf.mtext.retcode;
}
