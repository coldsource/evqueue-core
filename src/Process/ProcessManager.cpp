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

#include <Process/ProcessManager.h>
#include <Exception/Exception.h>
#include <global.h>
#include <Queue/QueuePool.h>
#include <WorkflowInstance/WorkflowInstance.h>
#include <Notification/Notifications.h>
#include <Configuration/ConfigurationEvQueue.h>
#include <Schedule/Retrier.h>
#include <API/Statistics.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <Logger/Logger.h>
#include <User/User.h>
#include <WorkflowInstance/Task.h>
#include <Process/ProcessExec.h>
#include <Process/tools_ipc.h>
#include <DB/DB.h>

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
string ProcessManager::logs_directory;
bool ProcessManager::logs_delete;
int ProcessManager::log_maxsize;

ProcessManager::ProcessManager()
{
	Configuration *config = ConfigurationEvQueue::GetInstance();
	const char *logs_delete_str;
	logs_directory = config->Get("processmanager.logs.directory");
	
	logs_delete = config->GetBool("processmanager.logs.delete");
	
	log_maxsize = config->GetSize("datastore.db.maxsize");
	
	// Create message queue
	msgqid = ipc_openq(ConfigurationEvQueue::GetInstance()->Get("core.ipc.qid").c_str());
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
	
	DB::StartThread();
	
	Logger::Log(LOG_NOTICE,"Forker started");
	
	while(1)
	{
		if(!qp->DequeueTask(queue_name,&workflow_instance,&task))
		{
			Logger::Log(LOG_NOTICE,"Shutdown in progress exiting Forker");
			
			DB::StopThread();
			
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
	
	DB::StartThread();
	
	Logger::Log(LOG_NOTICE,"Gatherer started");
	
	while(1)
	{
		int re = msgrcv(pm->msgqid,&msgbuf,sizeof(st_msgbuf::mtext),0,0);
		if(re<0)
		{
			if(errno==EINTR)
				continue; // Interrupted but we can still continue
			else
				break;
		}
		
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
			
			DB::StopThread();
			
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
			stdout_output =  read_log_file(pid,tid,STDOUT_FILENO);
			stderr_output =  read_log_file(pid,tid,STDERR_FILENO);
			log_output =  read_log_file(pid,tid,LOG_FILENO);
			
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
	DB::StopThread();
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
	const Task &task,
	pid_t tid
	)
{
	Configuration *config = ConfigurationEvQueue::GetInstance();
	
	ProcessExec proc(config->Get("processmanager.monitor.path"));
	
	// Compute task filename
	string task_filename;
	if(task.IsAbsolutePath())
		task_filename = task.GetPath();
	else
		task_filename = ConfigurationEvQueue::GetInstance()->Get("processmanager.tasks.directory")+"/"+task.GetPath();
	
	// Static process arguments
	proc.AddArgument(task_filename);
	proc.AddArgument(to_string(tid));
	
	auto task_arguments = task.GetArguments();
	for (int i = 0; i<task_arguments.size(); i++)
		proc.AddArgument(task_arguments.at(i));
	
	// Redirect to files
	proc.FileRedirect(STDOUT_FILENO,logs_directory+"/"+to_string(tid));
	
	if(task.GetMergeStderr())
		proc.FDRedirect(STDERR_FILENO,STDIN_FILENO);
	else
		proc.FileRedirect(STDERR_FILENO,logs_directory+"/"+to_string(tid));
	
	proc.FileRedirect(LOG_FILENO,logs_directory+"/"+to_string(tid));
	
	// Sanity check
	if(task.GetType()==task_type::SCRIPT && task.GetHost()!="" && !task.GetUseAgent())
		throw Exception("ProcessManager", "Script tasks cannot be remote without using evQueue agent");
	
	// Prepare monitor config
	map<string,string> monitor_config;
	monitor_config["core.ipc.qid"] = config->Get("core.ipc.qid");
	monitor_config["processmanager.agent.path"] = config->Get("processmanager.agent.path");
	monitor_config["processmanager.monitor.ssh_key"] = config->Get("processmanager.monitor.ssh_key");
	monitor_config["processmanager.monitor.ssh_path"] = config->Get("processmanager.monitor.ssh_path");
	monitor_config["processmanager.scripts.directory"] = config->Get("processmanager.scripts.directory");
	monitor_config["processmanager.scripts.delete"] = config->Get("processmanager.scripts.delete");
	
	monitor_config["monitor.ssh.useagent"] = task.GetUseAgent()?"yes":"no";
	monitor_config["monitor.ssh.host"] = task.GetHost();
	monitor_config["monitor.ssh.user"] = task.GetUser();
	monitor_config["monitor.wd"] = task.GetWorkingDirectory();
	monitor_config["monitor.task.type"] = task.GetTypeStr();
	
	proc.PipeMap(monitor_config);
	
	// Task arguments from WF
	vector<string> parameters_name;
	vector<string> parameters_value;
	task.GetParameters(parameters_name,parameters_value);
	
	if(task.GetParametersMode()==task_parameters_mode::CMDLINE)
	{
		for(int i=0;i<parameters_value.size();i++)
			proc.AddArgument(parameters_value.at(i));
	}
	
	map<string,string> env;
	env["EVQUEUE_IPC_QID"] = "0"; // Backward compatibility, to be removed
	env["EVQUEUE_ENV"] = "true";
	if(task.GetParametersMode()==task_parameters_mode::ENV)
	{
		for(int i=0;i<parameters_value.size();i++)
			env[parameters_name.at(i)] = parameters_value.at(i);
	}
	proc.PipeMap(env);
	
	// Send script content if needed
	if(task.GetType()==task_type::SCRIPT)
		proc.PipeString(task.GetScript());
	
	// Stdin
	proc.Pipe(task.GetStdin());
	
	pid_t pid = proc.Exec();
	
	if(pid<0)
		throw Exception("ProcessManager","Unable to execute task '"+task.GetPath()+"' : could not fork");
	
	if(pid==0)
	{
		// Could not properly call monitor
		ipc_send_exit_msg(ConfigurationEvQueue::GetInstance()->Get("core.ipc.qid").c_str(),1,tid,-1);
		exit(-1);
	}

	return pid;
}

bool ProcessManager::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	const string action = query->GetRootAttribute("action");
	
	if(action=="tail")
	{
		unsigned int tid = query->GetRootAttributeInt("tid");
		string type_str= query->GetRootAttribute("type");
		int type_int;
		if(type_str=="stdout")
			type_int = STDOUT_FILENO;
		else if(type_str=="stderr")
			type_int = STDERR_FILENO;
		else if(type_str=="log")
			type_int = LOG_FILENO;
		else
			throw Exception("ProcessManager", "Unknown log type : "+type_str);
		
		response->AppendText(tail_log_file(tid,type_int));
		
		return true;
	}
	
	return false;
}

char *ProcessManager::read_log_file(pid_t pid,pid_t tid,int log_fileno)
{
	string log_filename;
	if(log_fileno==STDOUT_FILENO)
		log_filename = logs_directory+"/"+to_string(tid)+".stdout";
	else if(log_fileno==STDERR_FILENO)
		log_filename = logs_directory+"/"+to_string(tid)+".stderr";
	else
		log_filename = logs_directory+"/"+to_string(tid)+".log";
	
	FILE *f;
	long log_size;
	bool log_truncated = false;
	char *output;
	
	f  = fopen(log_filename.c_str(),"r+");
	
	if(f)
	{
		// Get file size
		fseek(f,0,SEEK_END);
		log_size = ftell(f);
		
		if(log_size>log_maxsize)
		{
			log_truncated = true;
			if(ftruncate(fileno(f),log_maxsize)!=0)
				Logger::Log(LOG_WARNING,"[ ProcessManager ] Could not truncate return log file, truncate will be made on reading");
			
			fseek(f,0,SEEK_END);
			fwrite("...TRUNCATED...",1,15,f);
			
			log_size=log_maxsize+15;
		}
		
		// Read output log
		fseek(f,0,SEEK_SET);
		
		output = new char[log_size+1];
		if(fread(output,1,log_size,f)!=log_size)
			Logger::Log(LOG_WARNING,"[ ProcessManager ] Error reading output log for pid %d",pid);
		
		output[log_size] = '\0';
		
		fclose(f);
		
		for(int i=0;i<log_size;i++)
		{
			if(output[i]==0x19)
			{
				output[i] = '?'; // Remove buggy characters from output (not properly handled by xerces on Serialize)
				Logger::Log(LOG_WARNING, "[ ProcessManager ] Removed invalid character 0x19 (End of Medium) from output, pid %d, tid %d",pid, tid);
			}
		}
	}
	else
	{
		output = 0;
		
		Logger::Log(LOG_WARNING,"[ ProcessManager ] Could not read task output for pid %d",pid);
	}
	
	if(logs_delete)
		unlink(log_filename.c_str()); // Delete log file since it is not usefull anymore
	
	return output;
}

string ProcessManager::tail_log_file(pid_t tid,int log_fileno)
{
	string log_filename;
	if(log_fileno==STDOUT_FILENO)
		log_filename = logs_directory+"/"+to_string(tid)+".stdout";
	else if(log_fileno==STDERR_FILENO)
		log_filename = logs_directory+"/"+to_string(tid)+".stderr";
	else
		log_filename = logs_directory+"/"+to_string(tid)+".log";
	
	FILE *f;
	
	f  = fopen(log_filename.c_str(),"r+");
	if(!f)
		throw Exception("ProcessManager","Error opening log file");
	
	// Tail fail
	int size = ConfigurationEvQueue::GetInstance()->GetSize("processmanager.logs.tailsize");
	fseek(f,-size,SEEK_END);
	
	char output[size+1];
	int read_size = fread(output,1,size,f);
	output[read_size] = '\0';
	
	fclose(f);
	
	return string(output);
}

