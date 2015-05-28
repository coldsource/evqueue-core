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
#include <Configuration.h>
#include <Retrier.h>
#include <Statistics.h>
#include <Logger.h>

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

volatile bool ProcessManager::is_shutting_down=false;

ProcessManager::ProcessManager()
{
	Configuration *config = Configuration::GetInstance();
	const char *logs_delete_str;
	logs_directory = config->Get("processmanager.logs.directory");
	
	logs_delete = config->GetBool("processmanager.logs.delete");
	
	log_filename = new char[logs_directory.length()+16];
	
	// Create message queue
	msgqid = msgget(PROCESS_MANAGER_MSGQID,0700 | IPC_CREAT);
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
	delete[] log_filename;
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
	DOMNode *task;
	
	char queue_name[QUEUE_NAME_MAX_LEN+1];
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
			DOMNode *task;
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
	
	char *output;
	long log_size;
	bool workflow_terminated;
	pid_t pid,tid;
	int status;
	char retcode;
	FILE *f;
	st_msgbuf msgbuf;
	
	WorkflowInstance *workflow_instance;
	DOMNode *task;
	
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
		
		waitpid(pid,&status,0); // We only do this to avoid zombie processes (retcode has already been returned by the task monitor)
		
		if(msgbuf.type==1)
		{
			// Fetch task output in log file before releasing tid
			sprintf(pm->log_filename,"%s/%d.log",pm->logs_directory.c_str(),tid);
			f = fopen(pm->log_filename,"r");
			
			if(f)
			{
				// Get file size
				fseek(f,0,SEEK_END);
				log_size = ftell(f);
				
				// Read output log
				fseek(f,0,SEEK_SET);
				output = new char[log_size+1];
				fread(output,1,log_size,f);
				output[log_size] = '\0';
				
				fclose(f);
			}
			else
			{
				output = 0;
				
				Logger::Log(LOG_WARNING,"[ ProcessManager ] Could not read task output for pid %d",pid);
			}
			
			if(pm->logs_delete)
				unlink(pm->log_filename); // Delete log file since it is not usefull anymore
			
			// Get task informations
			if(!QueuePool::GetInstance()->TerminateTask(tid,&workflow_instance,&task))
			{
				Logger::Log(LOG_WARNING,"[ ProcessManager ] Got exit message from pid %d (tid %d) but could not get corresponding workflow instance",pid,tid);
				continue; // Oops task was not found, this can happen on resume when tables have been cleaned
			}
			
			if(output)
				workflow_instance->TaskStop(task,retcode,output,&workflow_terminated);
			else
				workflow_instance->TaskStop(task,-1,"[ ProcessManager ] Could not read task log, setting retcode to -1 to block subjobs",&workflow_terminated);
			
			if(workflow_terminated)
				delete workflow_instance;
			
			if(output)
				delete[] output;
		}
		
		if(msgbuf.type==2)
		{
			// Notification task
			if(tid==0)
			{
				if(retcode!=0)
					Logger::Log(LOG_WARNING,"Notification task %d returned code %d",pid,retcode);
				else
					Logger::Log(LOG_NOTICE,"Notification task %d executed successuflly",pid);
			}
			else if(tid==1)
				Logger::Log(LOG_WARNING,"Notification task %d was killed",pid);
			else if(tid==2)
				Logger::Log(LOG_WARNING,"Notification task %d timed out",pid);
			else if(tid==3)
				Logger::Log(LOG_ALERT,"Notification task %d could not be forked",pid);
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
