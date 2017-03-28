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
#include <Tasks.h>
#include <Task.h>
#include <tools.h>
#include <tools_ipc.h>

#include <mysql/mysql.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>

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
	
	// Start forker
	forker_thread_handle = thread(ProcessManager::Fork,this);
	
	// Start gatherer
	gatherer_thread_handle = thread(ProcessManager::Gather,this);
}

ProcessManager::~ProcessManager()
{
}

void *ProcessManager::Fork(ProcessManager *pm)
{
	// Block signals
	sigset_t signal_mask;
	sigemptyset(&signal_mask);
	sigaddset(&signal_mask, SIGINT);
	sigaddset(&signal_mask, SIGTERM);
	sigaddset(&signal_mask, SIGHUP);
	pthread_sigmask(SIG_BLOCK, &signal_mask, NULL);
	
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

void *ProcessManager::Gather(ProcessManager *pm)
{
	// Block signals
	sigset_t signal_mask;
	sigemptyset(&signal_mask);
	sigaddset(&signal_mask, SIGINT);
	sigaddset(&signal_mask, SIGTERM);
	sigaddset(&signal_mask, SIGHUP);
	pthread_sigmask(SIG_BLOCK, &signal_mask, NULL);
	
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
	forker_thread_handle.join();
	gatherer_thread_handle.join();
}

pid_t ProcessManager::ExecuteTask(
	const string &task_name,
	const vector<string> &parameters_name,
	const vector<string> &parameters_value,
	const string &stdin_parameter,
	pid_t tid,
	const string &host,
	const string &user
	)
{
	char buf[32],tid_str[16];
	char *task_name_c;
	int parameters_pipe[2];
	Task task;
	
	static const string tasks_directory = Configuration::GetInstance()->Get("processmanager.tasks.directory");
	static const string monitor_path = Configuration::GetInstance()->Get("processmanager.monitor.path");

	task = Tasks::GetInstance()->Get(task_name);

	if(stdin_parameter!="")
	{
		// Prepare pipe before fork() since we have STDIN input
		parameters_pipe[0] = -1;
		if(pipe(parameters_pipe)!=0)
			throw Exception("ProcessManager","Unable to execute task : could not create pipe");
	}

	pid_t pid;
	// Fork child
	pid = fork();

	if(pid==0)
	{
		setsid(); // This is used to avoid CTRL+C killing all child processes

		pid = getpid();
		sprintf(tid_str,"%d",tid);

		// Send QID to monitor
		Configuration *config = Configuration::GetInstance();
		setenv("EVQUEUE_IPC_QID",config->Get("core.ipc.qid").c_str(),true);

		// Compute task filename
		string task_filename;
		if(task.IsAbsolutePath())
			task_filename = task.GetBinary();
		else
			task_filename = tasks_directory+"/"+task.GetBinary();

		if(!task.GetWorkingDirectory().empty())
			setenv("EVQUEUE_WORKING_DIRECTORY",task.GetWorkingDirectory().c_str(),true);

		// Set SSH variables for remote execution if needed by task
		if(!task.GetHost().empty())
		{
			// Task is configured as distant
			setenv("EVQUEUE_SSH_HOST",task.GetHost().c_str(),true);

			if(!task.GetUser().empty())
				setenv("EVQUEUE_SSH_USER",task.GetUser().c_str(),true);
		}
		else if(host!="")
		{
			// Dynamic task execution is enabled
			setenv("EVQUEUE_SSH_HOST",host.c_str(),true);
			
			if(user!="")
				setenv("EVQUEUE_SSH_USER",user.c_str(),true);
		}

		if(getenv("EVQUEUE_SSH_HOST"))
		{
			// Set SSH config variables if SSH execution is asked
			setenv("EVQUEUE_SSH_PATH",config->Get("processmanager.monitor.ssh_path").c_str(),true);

			if(config->Get("processmanager.monitor.ssh_key").length()>0)
				setenv("EVQUEUE_SSH_KEY",config->Get("processmanager.monitor.ssh_key").c_str(),true);

			// Send agent path if needed
			if(task.GetUseAgent())
				setenv("EVQUEUE_SSH_AGENT",config->Get("processmanager.agent.path").c_str(),true);
		}

		if(stdin_parameter!="")
		{
			dup2(parameters_pipe[0],STDIN_FILENO);
			close(parameters_pipe[1]);
		}

		// Redirect output to log files
		int fno;

		fno = open_log_file(tid,LOG_FILENO);
		dup2(fno,LOG_FILENO);

		fno = open_log_file(tid,STDOUT_FILENO);
		dup2(fno,STDOUT_FILENO);

		if(!task.GetMergeStderr())
			fno = open_log_file(tid,STDERR_FILENO);

		dup2(fno,STDERR_FILENO);
		
		int parameters_count = parameters_name.size();
		int parameters_index;

		if(task.GetParametersMode()==task_parameters_mode::CMDLINE)
		{
			const char *args[parameters_count+4];

			args[0] = monitor_path.c_str();
			args[1] = task_filename.c_str();
			args[2] = tid_str;

			for(parameters_index=0;parameters_index<parameters_count;parameters_index++)
				args[parameters_index+3] = parameters_value[parameters_index].c_str();

			args[parameters_index+3] = (char *)0;

			execv(monitor_path.c_str(),(char * const *)args);

			Logger::Log(LOG_ERR,"Could not execute task monitor");
			tools_send_exit_msg(1,tid,-1);
			exit(-1);
		}
		else if(task.GetParametersMode()==task_parameters_mode::ENV)
		{
			for(parameters_index=0;parameters_index<parameters_count;parameters_index++)
			{
				if(parameters_name[parameters_index]!="")
					setenv(parameters_name[parameters_index].c_str(),parameters_value[parameters_index].c_str(),1);
				else
					setenv("DEFAULT_PARAMETER_NAME",parameters_value[parameters_index].c_str(),1);
			}

			execl(monitor_path.c_str(),monitor_path.c_str(),task_filename.c_str(),tid_str,(char *)0);

			Logger::Log(LOG_ERR,"Could not execute task monitor");
			tools_send_exit_msg(1,tid,-1);
			exit(-1);
		}

		tools_send_exit_msg(1,tid,-1);
		exit(-1);
	}

	if(pid>0)
	{
		if(stdin_parameter!="")
		{
			// We have STDIN data
			if(write(parameters_pipe[1],stdin_parameter.c_str(),stdin_parameter.length())!=stdin_parameter.length())
				Logger::Log(LOG_WARNING,"Unable to write parameters to pipe");

			close(parameters_pipe[0]);
			close(parameters_pipe[1]);
		}
	}

	if(pid<0)
		throw Exception("ProcessManager","Unable to execute task '"+task_name+"' : could not fork");

	return pid;
}

int ProcessManager::open_log_file(int tid, int fileno)
{
	static const string logs_directory = Configuration::GetInstance()->Get("processmanager.logs.directory");
	char *log_filename = new char[logs_directory.length()+32];

	if(fileno==STDOUT_FILENO)
		sprintf(log_filename,"%s/%d.stdout",logs_directory.c_str(),tid);
	else if(fileno==STDERR_FILENO)
		sprintf(log_filename,"%s/%d.stderr",logs_directory.c_str(),tid);
	else
		sprintf(log_filename,"%s/%d.log",logs_directory.c_str(),tid);

	int fno;
	fno=open(log_filename,O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR);

	delete[] log_filename;

	if(fno==-1)
	{
		Logger::Log(LOG_ERR,"Unable to open task output log file (%d)",fileno);
		tools_send_exit_msg(1,tid,-1);
		exit(-1);
	}

	return fno;
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
