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

#include <ProcessManager.h>
#include <Exception.h>
#include <global.h>
#include <QueuePool.h>
#include <WorkflowInstance.h>
#include <Notifications.h>
#include <Configuration.h>
#include <Retrier.h>
#include <Statistics.h>
#include <Logger.h>
#include <tools_ipc.h>

#include <pthread.h>
#include <mysql/mysql.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <signal.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <arpa/inet.h>

#include <string>

using namespace std;

volatile bool ProcessManager::is_shutting_down=false;

ProcessManager::ProcessManager()
{
	Configuration *config = Configuration::GetInstance();
	const char *logs_delete_str;
	logs_directory = config->Get("processmanager.logs.directory");
	
	logs_delete = config->GetBool("processmanager.logs.delete");
	
	// Create message queue
	msgqid = ipc_openq(Configuration::GetInstance()->Get("core.ipc.qid").c_str());
	if(msgqid==-1)
		throw Exception("ProcessManager","Unable to get message queue");
	
	pthread_attr_t thread_attr;
	
	// Start forker
	pthread_attr_init(&thread_attr);
	pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);
	pthread_create(&forker_thread_handle,&thread_attr,ProcessManager::Fork,this);
	pthread_setname_np(forker_thread_handle,"forker");
	
	// Start gatherer
	pthread_attr_init(&thread_attr);
	pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);
	pthread_create(&gatherer_thread_handle,&thread_attr,ProcessManager::Gather,this);
	pthread_setname_np(gatherer_thread_handle,"gatherer");
}

ProcessManager::~ProcessManager()
{
}

void *ProcessManager::Fork(void *process_manager)
{
	// Block signals
	sigset_t signal_mask;
	sigemptyset(&signal_mask);
	sigaddset(&signal_mask, SIGINT);
	sigaddset(&signal_mask, SIGTERM);
	sigaddset(&signal_mask, SIGHUP);
	pthread_sigmask(SIG_BLOCK, &signal_mask, NULL);
	
	ProcessManager *pm = (ProcessManager *)process_manager;
	QueuePool *qp = QueuePool::GetInstance();
	WorkflowInstance *workflow_instance;
	DOMElement task;
	
	string queue_name;
	bool workflow_terminated;
	pid_t pid,tid;
	
	mysql_thread_init();
	
	Logger::Log(LOG_NOTICE,"Forker started");
	
	while(1)
	{
		if(!qp->DequeueTask(queue_name,&workflow_instance,&task))
		{
			Logger::Log(LOG_NOTICE,"Shutdown in progress exiting Forker");
			
			mysql_thread_end();
			
			return 0; // Lock has been released because we are shutting down, nothing to execute
		}
		
		tid = qp->ExecuteTask(workflow_instance,task,queue_name,0); // Register task in QueuePool to get task ID
		
		pid = workflow_instance->TaskExecute(task,tid,&workflow_terminated);
		
		if(pid==-1)
		{
			// Unable to execute task, release task ID
			WorkflowInstance *workflow_instance;
			DOMElement task;
			QueuePool::GetInstance()->TerminateTask(tid,&workflow_instance,&task);
		}
		
		if(workflow_terminated)
			delete workflow_instance;
	}
}

void *ProcessManager::Gather(void *process_manager)
{
	// Block signals
	sigset_t signal_mask;
	sigemptyset(&signal_mask);
	sigaddset(&signal_mask, SIGINT);
	sigaddset(&signal_mask, SIGTERM);
	sigaddset(&signal_mask, SIGHUP);
	pthread_sigmask(SIG_BLOCK, &signal_mask, NULL);
	
	ProcessManager *pm = (ProcessManager *)process_manager;
	
	char *stdout_output,*stderr_output,*log_output;
	bool workflow_terminated;
	pid_t pid,tid;
	int status;
	char retcode;
	st_msgbuf msgbuf;
	
	WorkflowInstance *workflow_instance;
	DOMElement task;
	
	mysql_thread_init();
	
	Logger::Log(LOG_NOTICE,"Gatherer started");
	
	while(msgrcv(pm->msgqid,&msgbuf,sizeof(st_msgbuf::mtext),0,0)>0)
	{
		pid = msgbuf.mtext.pid;
		tid = msgbuf.mtext.tid;
		retcode = msgbuf.mtext.retcode;
		
		if(pid==0)
		{
			if(!is_shutting_down)
			{
				Logger::Log(LOG_WARNING,"Received shutdown signal but no shutdown in progress, ignoring...");
				continue;
			}
			
			Logger::Log(LOG_NOTICE,"Shutdown in progress exiting Gatherer");
			
			mysql_thread_end();
			
			return 0; // Shutdown requested
		}
		
		if(msgbuf.type==3)
		{
			if(!QueuePool::GetInstance()->GetTask(tid,&workflow_instance,&task))
				continue;
			
			workflow_instance->TaskUpdateProgression(task,msgbuf.mtext.retcode);
			
			continue; // Not process is terminated, skip waitpid
		}
		
		waitpid(pid,&status,0); // We only do this to avoid zombie processes (retcode has already been returned by the task monitor)
		
		if(msgbuf.type==1)
		{
			// Fetch task output in log files before releasing tid
			stdout_output =  read_log_file(pm,pid,tid,STDOUT_FILENO);
			stderr_output =  read_log_file(pm,pid,tid,STDERR_FILENO);
			log_output =  read_log_file(pm,pid,tid,LOG_FILENO);
			
			// Get task informations
			if(!QueuePool::GetInstance()->TerminateTask(tid,&workflow_instance,&task))
			{
				if(stdout_output)
					delete[] stdout_output;
			
				if(stderr_output)
					delete[] stderr_output;
				
				if(log_output)
					delete[] log_output;
				
				Logger::Log(LOG_WARNING,"[ ProcessManager ] Got exit message from pid %d (tid %d) but could not get corresponding workflow instance",pid,tid);
				continue; // Oops task was not found, this can happen on resume when tables have been cleaned
			}
			
			if(stdout_output)
				workflow_instance->TaskStop(task,retcode,stdout_output,stderr_output,log_output,&workflow_terminated);
			else
				workflow_instance->TaskStop(task,-1,"[ ProcessManager ] Could not read task log, setting retcode to -1 to block subjobs",stderr_output,log_output,&workflow_terminated);
			
			if(workflow_terminated)
				delete workflow_instance;
			
			if(stdout_output)
				delete[] stdout_output;
			
			if(stderr_output)
				delete[] stderr_output;
			
			if(log_output)
				delete[] log_output;
		}
		
		if(msgbuf.type==2)
		{
			// Notification task
			Notifications::GetInstance()->Exit(pid,tid,retcode);
		}
	}
	
	Logger::Log(LOG_CRIT,"[ ProcessManager ] msgrcv() returned error %d, exiting Gatherer",errno);
	mysql_thread_end();
	return 0;
}

void ProcessManager::Shutdown(void)
{
	is_shutting_down = true;
	
	QueuePool *qp = QueuePool::GetInstance();
	qp->Shutdown(); // Shutdown forker
	
	st_msgbuf msgbuf;
	msgbuf.type = 1;
	memset(&msgbuf.mtext,0,sizeof(st_msgbuf::mtext));
	msgsnd(msgqid,&msgbuf,sizeof(st_msgbuf::mtext),0); // Shutdown gatherer
}

void ProcessManager::WaitForShutdown(void)
{
	pthread_join(forker_thread_handle,0);
	pthread_join(gatherer_thread_handle,0);
}

char *ProcessManager::read_log_file(ProcessManager *pm,pid_t pid,pid_t tid,int fileno)
{
	string log_filename;
	if(fileno==STDOUT_FILENO)
		log_filename = pm->logs_directory+"/"+to_string(tid)+".stdout";
	else if(fileno==STDERR_FILENO)
		log_filename = pm->logs_directory+"/"+to_string(tid)+".stderr";
	else
		log_filename = pm->logs_directory+"/"+to_string(tid)+".log";
	
	FILE *f;
	long log_size;
	char *output;
	
	f  = fopen(log_filename.c_str(),"r");
	
	if(f)
	{
		// Get file size
		fseek(f,0,SEEK_END);
		log_size = ftell(f);
		
		// Read output log
		fseek(f,0,SEEK_SET);
		output = new char[log_size+1];
		if(fread(output,1,log_size,f)!=log_size)
			Logger::Log(LOG_WARNING,"[ ProcessManager ] Error reading output log for pid %d",pid);
		output[log_size] = '\0';
		
		fclose(f);
	}
	else
	{
		output = 0;
		
		Logger::Log(LOG_WARNING,"[ ProcessManager ] Could not read task output for pid %d",pid);
	}
	
	if(pm->logs_delete)
		unlink(log_filename.c_str()); // Delete log file since it is not usefull anymore
	
	return output;
}
