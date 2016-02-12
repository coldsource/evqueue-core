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
st_msgbuf msgbuf;

void signal_callback_handler(int signum)
{
	if(signum==SIGTERM)
	{
		// Forward signal to our child
		fprintf(stderr,"evqueue_notification_monitor : received SIGTERM, killing task...\n");
		if(pid)
			kill(pid,SIGKILL);
		
		msgbuf.mtext.tid = 1; // Killed
	}
	else if(signum==SIGALRM)
	{
		fprintf(stderr,"evqueue_notification_monitor : task timed out...\n");
		if(pid)
			kill(pid,SIGKILL);
		
		msgbuf.mtext.tid = 2; // Timed out
	}
}

int main(int argc,char ** argv)
{
	if(argc<=5)
		return -1;
	
	// Catch signals
	signal(SIGTERM,signal_callback_handler);
	signal(SIGALRM,signal_callback_handler);
	
	// Unblock SIGTERM (blocked by evqueue)
	sigset_t signal_mask;
	sigemptyset(&signal_mask);
	sigaddset(&signal_mask, SIGTERM);
	sigprocmask(SIG_UNBLOCK,&signal_mask,0);
	
	// Create message queue
	int msgqid = ipc_openq(getenv("EVQUEUE_IPC_QID"));
	if(msgqid==-1)
		return -1;
	
	int timeout = atoi(argv[2]);
	char *cmd_filename = argv[1];
	char *wfi_id = argv[3];
	char *wfi_errors = argv[4];
	char *unix_socket_path = argv[5];
	
	int status;
	
	msgbuf.type = 2;
	msgbuf.mtext.pid = getpid();
	msgbuf.mtext.tid = 0; // Normal condition
	
	pid = fork();
	
	if(pid==0)
	{
		const char *working_directory = getenv("EVQUEUE_WORKING_DIRECTORY");
		
		if(working_directory)
			chdir(working_directory);
		
		status = execl(cmd_filename,cmd_filename,wfi_id,wfi_errors,unix_socket_path,(char *)0);
		
		fprintf(stderr,"Unable to execute command '%s'. execv() returned %d\n",cmd_filename,status);
		return -1;
	}
	
	if(pid<0)
	{
		fprintf(stderr,"Unable to execute fork\n");
		
		msgbuf.mtext.retcode = -1;
		msgbuf.mtext.tid = 3; // Unable to fork
	}
	else
	{
		if(timeout>0)
			alarm(timeout);
		
		waitpid(pid,&status,0);
		if(WIFEXITED(status))
			msgbuf.mtext.retcode = WEXITSTATUS(status);
		else
			msgbuf.mtext.retcode = -1;
	}
	
	msgsnd(msgqid,&msgbuf,sizeof(st_msgbuf::mtext),0); // Notify evqueue
	
	return msgbuf.mtext.retcode;
}
