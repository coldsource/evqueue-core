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
#include <Process/tools_ipc.h>
#include <Process/DataSerializer.h>
#include <Configuration/Configuration.h>
#include <Exception/Exception.h>
#include <Process/ProcessExec.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>

#include <map>
#include <string>

using namespace std;

pid_t pid = 0;

bool send_progress_message(int msgqid,const char* buf,pid_t tid);

void signal_callback_handler(int signum)
{
	if(signum==SIGTERM)
	{
		// Forward signal to our child
		fprintf(stderr,"evqueue_monitor : received SIGTERM, killing task...\n");
		if(pid)
		{
			pid_t pgid = getpgid(pid);
			kill(-pgid,SIGKILL);
		}
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
	
	pid_t tid = atoi(argv[2]);
	int msgqid = -1;
	
	try
	{
		// Unserialize configuration that was piped on stdin
		map<string,string> config_map;
		if(!DataSerializer::Unserialize(STDIN_FILENO,config_map))
			throw Exception("evqueue_monitor","Could not read configuration");
		
		// Load config
		Configuration *config = new Configuration(config_map);
		
		// Create message queue now we have read configuration
		msgqid = ipc_openq(config->Get("core.ipc.qid").c_str());
		if(msgqid==-1)
			return -1; // Unable to notify evQueue daemon of our exit... Daemon is probably not running anyway
			
		// Unserialize ENV that was piped on stdin
		map<string,string> env_map;
		if(!DataSerializer::Unserialize(STDIN_FILENO,env_map))
			throw Exception("evqueue_monitor","Could not read environment");
		
		// Get script if needed
		string script;
		if(config->Get("monitor.task.type")=="SCRIPT")
		{
			if(!DataSerializer::Unserialize(STDIN_FILENO,script))
				throw Exception("evqueue_monitor","Could not read script");
		}
		
		// Task might read stdin slowly (which is blocking), so read the full data here to release the core engine
		// The stream the data from here to the child process
		string stdin_data;
		char stdin_buf[4096];
		int read_bytes;
		while((read_bytes = read(STDIN_FILENO,stdin_buf,4096)) > 0)
			stdin_data.append(stdin_buf,read_bytes);
		
		// Prepare to execute task
		ProcessExec proc;
		
		int stdout_fd = -1, log_fd = -1;
		bool use_ssh = config->Get("monitor.ssh.host")!="";
		if(use_ssh)
		{
			proc.SetPath(config->Get("processmanager.monitor.ssh_path"));
			
			// SSH key
			if(config->Get("processmanager.monitor.ssh_key")!="")
			{
				proc.AddArgument("-i");
				proc.AddArgument(config->Get("processmanager.monitor.ssh_key"));
			}
			
			if(config->Get("monitor.ssh.user")!="")
				proc.AddArgument(config->Get("monitor.ssh.user")+"@"+config->Get("monitor.ssh.host"));
			else
				proc.AddArgument(config->Get("monitor.ssh.host"));
			
			if(config->Get("monitor.wd")!="")
				proc.AddArgument("cd "+config->Get("monitor.wd")+";");
			
			if(config->GetBool("monitor.ssh.useagent"))
			{
				proc.AddArgument(config->Get("processmanager.agent.path"));
				stdout_fd = proc.ParentRedirect(STDOUT_FILENO);
			}
			
			proc.AddArgument(argv[1]);
			
			proc.PipeMap(config_map);
			proc.PipeMap(env_map);
			if(config->Get("monitor.task.type")=="SCRIPT")
				proc.PipeString(script);
			proc.Pipe(stdin_data);
		}
		else
		{
			if(config->Get("monitor.task.type")=="BINARY")
				proc.SetPath(argv[1]);
			else if(config->Get("monitor.task.type")=="SCRIPT")
				proc.SetScript(config->Get("processmanager.scripts.directory"),script);
			
			// Register ENV
			for(auto it = env_map.begin();it!=env_map.end();++it)
				setenv(it->first.c_str(),it->second.c_str(),true);
			
			if(config->Get("monitor.wd")!="")
			{
				if(chdir(config->Get("monitor.wd").c_str())!=0)
					throw Exception("evqueue_monitor","Unable to change working directory to '"+config->Get("monitor.wd")+"'");
			}
			
			log_fd = proc.ParentRedirect(LOG_FILENO);
			proc.Pipe(stdin_data);
		}
		
		// Task arguments
		for(int i=3;i<argc;i++)
			proc.AddArgument(argv[i],use_ssh);
		
		pid = proc.Exec();
		if(pid<0)
			throw Exception("evqueue_monitor","Unable to execute command '"+string(argv[1])+"', fork() returned error");
		else if(pid==0)
			throw Exception("evqueue_monitor","Unable to execute command '"+string(argv[1])+"', execv() returned error");
		
		st_msgbuf msgbuf;
		msgbuf.type = 1;
		msgbuf.mtext.pid = getpid();
		msgbuf.mtext.tid = tid;
		
		if(use_ssh && config->GetBool("monitor.ssh.useagent"))
		{
			// When using evqueue agent over SSH we receive 3 FDs multiplexed on stdout
			
			char line_buf[4096];
			int line_buf_size = 0;
	
			// Demultiplex Data
			FILE *log_in = fdopen(stdout_fd,"r");
			while(1)
			{
				char buf[4096];
				int read_size,data_size,data_fd;
				
				// Read file descriptor
				read_size = fread(buf,1,2,log_in);
				if(read_size==0)
					break;
				
				if(read_size!=2)
				{
					fprintf(stderr,"Corrupted data received from evqueue agent\n");
					break;
				}
				
				buf[read_size] = '\0';
				data_fd = atoi(buf);
				
				// Read data length
				read_size = fread(buf,1,9,log_in);
				if(read_size!=9)
				{
					fprintf(stderr,"Corrupted data received from evqueue agent\n");
					break;
				}
				
				buf[read_size] = '\0';
				data_size = atoi(buf);
				
				if(data_size>4096)
				{
					fprintf(stderr,"Corrupted data received from evqueue agent\n");
					break;
				}
				
				read_size = fread(buf,1,data_size,log_in);
				if(read_size!=data_size)
				{
					fprintf(stderr,"Corrupted data received from evqueue agent\n");
					break;
				}
				
				if(data_fd==LOG_FILENO)
				{
					// Parse evqueue log
					for(int i=0;i<read_size;i++)
					{
						line_buf[line_buf_size++] = buf[i];
						if(buf[i]=='\n' || line_buf_size==4095)
						{
							line_buf[line_buf_size] = '\0';
							
							if(strcmp(line_buf,"\\close\n")==0)
								break;
							
							if(!send_progress_message(msgqid,line_buf,tid))
							{
								if(line_buf[0]=='%')
								{
									if(write(LOG_FILENO,line_buf+1,line_buf_size-1)!=line_buf_size-1)
										fprintf(stderr,"Error writing to log file\n");
								}
								else
								{
									if(write(LOG_FILENO,line_buf,line_buf_size)!=line_buf_size)
										fprintf(stderr,"Error writing to log file\n");
								}
							}
							
							line_buf_size = 0;
						}
					}
				}
				else // Directly forward stdout and sterr
				{
					if(write(data_fd,buf,read_size)!=read_size)
						fprintf(stderr,"Error writing to fd %d\n",data_fd);
				}
			}
		}
		else if(!use_ssh)
		{
			// Running locally, stdout and stderr are already handled, just parse evqueue log
		
			char buf[4096];
			FILE *log_in = fdopen(log_fd,"r");
			FILE *log_out = fdopen(LOG_FILENO,"w");
			while(fgets(buf,4096,log_in))
			{
				if(strcmp(buf,"\\close\n")==0)
					break;
				
				if(!send_progress_message(msgqid,buf,tid))
				{
					if(buf[0]=='%')
						fputs(buf+1,log_out);
					else
						fputs(buf,log_out);
				}
			}
		}
		
		
		int status;
		waitpid(pid,&status,0);
		
		if(config->Get("monitor.task.type")=="SCRIPT" && config->GetBool("processmanager.scripts.delete"))
			unlink(proc.GetPath().c_str());
		
		if(WIFEXITED(status))
			msgbuf.mtext.retcode = WEXITSTATUS(status);
		else
			msgbuf.mtext.retcode = -1;
		
		msgsnd(msgqid,&msgbuf,sizeof(st_msgbuf::mtext),0); // Notify evqueue
		
		return msgbuf.mtext.retcode;
	}
	catch(Exception &e)
	{
		fprintf(stderr,"evqueue_monitor: %s\n",e.error.c_str());
		
		if(msgqid!=-1)
		{
			st_msgbuf msgbuf;
			msgbuf.type = 1;
			msgbuf.mtext.pid = getpid();
			msgbuf.mtext.tid = tid;
			msgbuf.mtext.retcode = -1;
			msgsnd(msgqid,&msgbuf,sizeof(st_msgbuf::mtext),0); // Notify evqueue
		}
		
		return -1;
	}
}

bool send_progress_message(int msgqid,const char* buf,pid_t tid)
{
	if(buf[0]!='%' || buf[1]=='%')
		return  false;
	
	int prct = atoi(buf+1);
	if(prct<0)
		prct = 0;
	else if(prct>100)
		prct = 100;
	
	st_msgbuf msgbuf_progress;
	msgbuf_progress.type = 3;
	msgbuf_progress.mtext.pid = getpid();
	msgbuf_progress.mtext.tid = tid;
	msgbuf_progress.mtext.retcode = prct;
	
	msgsnd(msgqid,&msgbuf_progress,sizeof(st_msgbuf::mtext),0);
	
	return  true;
}
