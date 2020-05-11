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

#include <Process/Forker.h>
#include <Process/tools_proc.h>
#include <Process/DataSerializer.h>
#include <Process/DataPiper.h>
#include <Process/tools_ipc.h>
#include <Process/Monitor.h>
#include <Process/NotificationMonitor.h>
#include <Configuration/ConfigurationEvQueue.h>
#include <Logger/Logger.h>
#include <Exception/Exception.h>

#include <sys/prctl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>

#include <vector>

using namespace std;

Forker *Forker::instance = 0;

void Forker::signal_callback_handler(int signum)
{
	if(signum==SIGCHLD)
	{
		int status;
		wait(&status);
	}
	
	if(signum==SIGTERM)
		close(Forker::GetInstance()->pipe_evq_to_forker[0]);
}

Forker::Forker()
{
	if(pipe(pipe_evq_to_forker)==-1)
		throw Exception("Forker","Could not create internal communication pipe");
	
	if(pipe(pipe_forker_to_evq)==-1)
		throw Exception("Forker","Could not create internal communication pipe");
	
	
	pipes_directory = ConfigurationEvQueue::GetInstance()->Get("forker.pipes.directory");
	node_name = ConfigurationEvQueue::GetInstance()->Get("cluster.node.name");
	
	instance = this;
}

pid_t Forker::Start()
{
	pid_t forker_pid = fork();
	if(forker_pid==0)
	{
		setsid(); // This is used to avoid CTRL+C killing all child processes
		
		// We want to die if evqueue daemon exists (for any reason)
		prctl(PR_SET_PDEATHSIG, SIGTERM);
		
		// Change display in ps
		setproctitle("evq_forker");
		
		// Close other pipes ends
		close(pipe_evq_to_forker[1]);
		close(pipe_forker_to_evq[0]);
		
		signal(SIGCHLD,signal_callback_handler); // Reap children
		signal(SIGTERM,signal_callback_handler); // Term cleanly
		
		while(true)
		{
			string type;
			if(!DataSerializer::Unserialize(pipe_evq_to_forker[0], type))
			{
				syslog(LOG_NOTICE, "forker: daemon is gone, exiting...");
				return 0; // Other end of the pipe has been close, daemon is terminating
			}
			
			string pipe_path;
			if(!DataSerializer::Unserialize(pipe_evq_to_forker[0], pipe_path))
			{
				syslog(LOG_CRIT, "forker: could not read from communication pipe, exiting...");
				return 0;
			}
			
			pid_t proc_pid;
			
			int fd = open(pipe_path.c_str(),O_RDONLY);
			if(fd<0)
				proc_pid = -2;
			else
			{
				proc_pid = fork();
				
				if(proc_pid==0)
				{
					setsid(); // This is used to avoid CTRL+C killing all child processes
					
					// Reset signal handlers
					signal(SIGCHLD,SIG_DFL);
					signal(SIGTERM,SIG_DFL);
					
					// Close communiction pipes with evqueue
					close(pipe_evq_to_forker[0]);
					close(pipe_forker_to_evq[1]);
					
					// Change display in ps
					setproctitle(type.c_str());
					
					int retcode = -2;
					if(type=="evq_monitor")
					{
						Monitor monitor(fd);
						retcode = monitor.main();
					}
					else if(type=="evq_nf_monitor")
					{
						NotificationMonitor notif_monitor(fd);
						retcode = notif_monitor.main();
					}
					
					return 0;
				}
				
				close(fd);
			}
			
			string pid_str = DataSerializer::Serialize(proc_pid);
			if(write(pipe_forker_to_evq[1],pid_str.c_str(),pid_str.length())!=pid_str.length())
				syslog(LOG_CRIT, "Forker: could not write to communication pipe");
		}
	}
	
	// Close other pipes ends
	close(pipe_evq_to_forker[0]);
	close(pipe_forker_to_evq[1]);
	
	return forker_pid;
}

pid_t Forker::Execute(const string &type, const string &data)
{
	unique_lock<mutex> llock(lock);
	
	string pipe_path = pipes_directory+"/evq_forker_fifo_"+node_name+"_"+to_string(pipe_id++);
	
	string pipe_data;
	pipe_data += DataSerializer::Serialize(type);
	pipe_data += DataSerializer::Serialize(pipe_path);
	
	
	mkfifo(pipe_path.c_str(),0600);
	int fd = open(pipe_path.c_str(),O_RDWR);
	if(fd<0)
	{
		Logger::Log(LOG_CRIT, "Could not open FIFO "+pipe_path+", could not start "+type);
		return -1;
	}
	
	if(write(pipe_evq_to_forker[1], pipe_data.c_str(), pipe_data.length())!=pipe_data.length())
	{
		Logger::Log(LOG_CRIT, "Could not contact forker through communication pipe");
		return -1;
	}
	
	pid_t proc_pid;
	if(!DataSerializer::Unserialize(pipe_forker_to_evq[0], &proc_pid))
	{
		Logger::Log(LOG_CRIT, "Could not read PID from forker FIFO "+pipe_path);
		return -1;
	}
	
	unlink(pipe_path.c_str());
	
	if(proc_pid>0)
		DataPiper::GetInstance()->PipeData(fd, data);
	
	if(proc_pid==-1)
		Logger::Log(LOG_CRIT, "Forker: could not fork()");
	if(proc_pid==-2)
		Logger::Log(LOG_CRIT, "Forker: could not open FIFO "+pipe_path+", could not start "+type);
	
	return proc_pid;
}
