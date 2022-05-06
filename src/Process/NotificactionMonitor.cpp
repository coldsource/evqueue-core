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

#include <Process/NotificationMonitor.h>
#include <Process/DataSerializer.h>
#include <Process/tools_ipc.h>
#include <Process/ProcessExec.h>
#include <Configuration/ConfigurationEvQueue.h>
#include <global.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string>
#include <vector>

using namespace std;

pid_t pid = 0;
st_msgbuf msgbuf;

static void signal_callback_handler(int signum)
{
	if(signum==SIGTERM)
	{
		// Forward signal to our child
		fprintf(stderr,"evq_nf_monitor : received SIGTERM, killing task...\n");
		if(pid)
			kill(pid,SIGKILL);
		
		msgbuf.mtext.tid = 1; // Killed
	}
	else if(signum==SIGALRM)
	{
		fprintf(stderr,"evq_nf_monitor : task timed out...\n");
		if(pid)
			kill(pid,SIGKILL);
		
		msgbuf.mtext.tid = 2; // Timed out
	}
}

int NotificationMonitor::main()
{
	ConfigurationEvQueue *config = ConfigurationEvQueue::GetInstance();
	
	// Create message queue
	int msgqid = ipc_openq(config->Get("core.ipc.qid").c_str());
	if(msgqid==-1)
		return -1;
	
	memset(&msgbuf, 0, sizeof(st_msgbuf));
	
	vector<string> args;
	DataSerializer::Unserialize(fd, args);
	
	string plugin_conf;
	DataSerializer::Unserialize(fd, plugin_conf);
	
	int nid = stoi(args[0]);
	string uid = args[1];
	int timeout = stoi(args[3]);
	
	// Redirect to files
	string logs_directory = config->Get("processmanager.logs.directory");
	ProcessExec::SelfFileRedirect(STDERR_FILENO,logs_directory+"/notif_"+uid+"_"+to_string(nid));
	
	// Catch signals
	signal(SIGTERM,signal_callback_handler);
	signal(SIGALRM,signal_callback_handler);
	
	int status;
	
	msgbuf.type = 2;
	msgbuf.mtext.pid = getpid();
	msgbuf.mtext.tid = 0; // Normal condition
	
	ProcessExec proc(args[2]);
	for(int i=4;i<args.size();i++)
		proc.AddArgument(args[i]);
	proc.Pipe(plugin_conf);
	proc.SetWorkingDirectory(config->Get("notifications.tasks.directory"));
	
	pid = proc.Exec();
	
	if(pid==0)
		return -2; // We are in child but execution failed, die with error code
	
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
