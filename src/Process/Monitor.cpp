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

#include <Process/Monitor.h>
#include <global.h>
#include <Process/tools_ipc.h>
#include <Process/DataSerializer.h>
#include <Configuration/Configuration.h>
#include <Configuration/ConfigurationEvQueue.h>
#include <Exception/Exception.h>
#include <Process/ProcessExec.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <syslog.h>

#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>

#include <string>
#include <map>
#include <vector>

static pid_t pid = 0;

static void signal_callback_handler(int signum)
{
	if(signum==SIGTERM)
	{
		// Forward signal to our child
		fprintf(stderr,"evq_monitor : received SIGTERM, killing task...\n");
		if(pid)
		{
			pid_t pgid = getpgid(pid);
			kill(-pgid,SIGKILL);
		}
	}
}

using namespace std;

int Monitor::main()
{
	ConfigurationEvQueue *config = ConfigurationEvQueue::GetInstance();
	
	// Catch signals
	signal(SIGTERM,signal_callback_handler);
	
	// Unblock SIGTERM (blocked by evqueue)
	sigset_t signal_mask;
	sigemptyset(&signal_mask);
	sigaddset(&signal_mask, SIGTERM);
	sigprocmask(SIG_UNBLOCK,&signal_mask,0);
	
	// Create message queue now we have read configuration
	int msgqid = ipc_openq(config->Get("core.ipc.qid").c_str());
	if(msgqid==-1)
		return -1; // Unable to notify evQueue daemon of our exit... Daemon is probably not running anyway
	
	pid_t tid;
	
	st_msgbuf msgbuf;
	memset(&msgbuf,0,sizeof(st_msgbuf));
	msgbuf.type = 1;
	msgbuf.mtext.pid = getpid();
	msgbuf.mtext.tid = -1;
	
	openlog("evq_monitor",0,LOG_DAEMON);
	
	try
	{
		string task_filename;
		if(!DataSerializer::Unserialize(fd, task_filename))
			throw Exception("evq_monitor","Could not read task filename");
		
		if(!DataSerializer::Unserialize(fd, &tid))
			throw Exception("evq_monitor","Could not read tid");
		
		msgbuf.mtext.tid = tid;
		
		// Redirect to files
		string logs_directory = config->Get("processmanager.logs.directory");
		ProcessExec::SelfFileRedirect(STDOUT_FILENO,logs_directory+"/"+to_string(tid));
		ProcessExec::SelfFileRedirect(STDERR_FILENO,logs_directory+"/"+to_string(tid));
		ProcessExec::SelfFileRedirect(LOG_FILENO,logs_directory+"/"+to_string(tid));
		
		vector<string> task_arguments;
		DataSerializer::Unserialize(fd, task_arguments);
		
		// Unserialize configuration that was piped on stdin
		map<string,string> config_map;
		if(!DataSerializer::Unserialize(fd,config_map))
			throw Exception("evq_monitor","Could not read configuration");
		
		// Load config
		Configuration monitor_config(config_map);
		
		// Unserialize ENV that was piped on stdin
		map<string,string> env_map;
		if(!DataSerializer::Unserialize(fd,env_map))
			throw Exception("evq_monitor","Could not read environment");
		
		// Get script if needed
		string script;
		if(monitor_config.Get("monitor.task.type")=="SCRIPT")
		{
			if(!DataSerializer::Unserialize(fd,script))
				throw Exception("evq_monitor","Could not read script");
		}
		
		// Task might read stdin slowly (which is blocking), so read the full data here to release the core engine
		// Then stream the data from here to the child process
		string stdin_data;
		char stdin_buf[4096];
		int read_bytes;
		while((read_bytes = read(fd,stdin_buf,4096)) > 0)
			stdin_data.append(stdin_buf,read_bytes);
		
		// Done reading, close FD
		close(fd);
		
		// Prepare to execute task
		ProcessExec proc;
		
		int stdout_fd = -1, log_fd = -1;
		bool use_ssh = monitor_config.Get("monitor.ssh.host")!="";
		if(use_ssh)
		{
			proc.SetPath(config->Get("processmanager.monitor.ssh_path"));
			
			// SSH key
			if(config->Get("processmanager.monitor.ssh_key")!="")
			{
				proc.AddArgument("-i");
				proc.AddArgument(config->Get("processmanager.monitor.ssh_key"));
			}
			
			if(monitor_config.Get("monitor.ssh.user")!="")
				proc.AddArgument(monitor_config.Get("monitor.ssh.user")+"@"+monitor_config.Get("monitor.ssh.host"));
			else
				proc.AddArgument(monitor_config.Get("monitor.ssh.host"));
			
			if(monitor_config.Get("monitor.wd")!="")
				proc.AddArgument("cd "+monitor_config.Get("monitor.wd")+";");
			
			if(monitor_config.GetBool("monitor.ssh.useagent"))
			{
				proc.AddArgument(config->Get("processmanager.agent.path"));
				stdout_fd = proc.ParentRedirect(STDOUT_FILENO);
			}
			
			proc.AddArgument(task_filename);
			
			proc.PipeMap(config_map);
			proc.PipeMap(env_map);
			if(monitor_config.Get("monitor.task.type")=="SCRIPT")
				proc.PipeString(script);
			proc.Pipe(stdin_data);
		}
		else
		{
			if(monitor_config.Get("monitor.task.type")=="BINARY")
				proc.SetPath(task_filename);
			else if(monitor_config.Get("monitor.task.type")=="SCRIPT")
				proc.SetScript(config->Get("processmanager.scripts.directory"),script);
			
			// Register ENV
			for(auto it = env_map.begin();it!=env_map.end();++it)
				setenv(it->first.c_str(),it->second.c_str(),true);
			
			if(monitor_config.Get("monitor.wd")!="")
			{
				if(chdir(monitor_config.Get("monitor.wd").c_str())!=0)
					throw Exception("evq_monitor","Unable to change working directory to '"+monitor_config.Get("monitor.wd")+"'");
			}
			
			log_fd = proc.ParentRedirect(LOG_FILENO);
			proc.Pipe(stdin_data);
		}
		
		if(monitor_config.Get("monitor.merge_stderr")=="yes")
			proc.FDRedirect(STDERR_FILENO,STDIN_FILENO);
		
		// Task arguments
		for(int i=0;i<task_arguments.size();i++)
			proc.AddArgument(task_arguments.at(i),use_ssh);
		
		pid = proc.Exec();
		if(pid<0)
			throw Exception("evq_monitor","Unable to execute command '"+task_filename+"', fork() returned error");
		else if(pid==0)
			throw Exception("evq_monitor","Unable to execute command '"+task_filename+"', execv() returned error");
		
		if(use_ssh && monitor_config.GetBool("monitor.ssh.useagent"))
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
							
							if(!ipc_send_progress_message(msgqid,line_buf,tid))
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
				
				if(!ipc_send_progress_message(msgqid,buf,tid))
				{
					if(buf[0]=='%')
						fputs(buf+1,log_out);
					else
						fputs(buf,log_out);
				}
			}
			fclose(log_in);
			fclose(log_out);
		}
		
		
		int status;
		waitpid(pid,&status,0);
		
		if(monitor_config.Get("monitor.task.type")=="SCRIPT" && config->GetBool("processmanager.scripts.delete"))
			unlink(proc.GetPath().c_str());
		
		if(WIFEXITED(status))
			msgbuf.mtext.retcode = WEXITSTATUS(status);
		else
			msgbuf.mtext.retcode = -1;
		
		// Notify evqueue
		if(msgsnd(msgqid,&msgbuf,sizeof(st_msgbuf::mtext),0)==-1)
			syslog(LOG_CRIT, "evq_monitor: failed to send daemon notification");
		
		return msgbuf.mtext.retcode;
	}
	catch(Exception &e)
	{
		syslog(LOG_ERR,"%s",e.error.c_str());
		fprintf(stderr,"evq_monitor: %s\n",e.error.c_str());
		
		msgbuf.mtext.retcode = -1;
		msgsnd(msgqid,&msgbuf,sizeof(st_msgbuf::mtext),0); // Notify evqueue
		
		return -1;
	}
}
