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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <errno.h>

#include <mysql/mysql.h>

#include <Logger.h>
#include <Queue.h>
#include <QueuePool.h>
#include <Workflows.h>
#include <WorkflowInstance.h>
#include <WorkflowInstances.h>
#include <ConfigurationReader.h>
#include <Exception.h>
#include <Retrier.h>
#include <WorkflowScheduler.h>
#include <WorkflowSchedule.h>
#include <global.h>
#include <ProcessManager.h>
#include <DB.h>
#include <Statistics.h>
#include <Tasks.h>
#include <RetrySchedules.h>
#include <GarbageCollector.h>
#include <handle_connection.h>

#include <xqilla/xqilla-dom3.hpp>

int listen_socket;

void signal_callback_handler(int signum)
{
	if(signum==SIGINT || signum==SIGTERM)
	{
		// Shutdown requested
		// Close main listen socket, this will release accept() loop
		close(listen_socket);
	}
	else if(signum==SIGHUP)
	{
		Logger::Log(LOG_NOTICE,"Got SIGHUP, reloading scheduler configuration");
		
		WorkflowScheduler *scheduler = WorkflowScheduler::GetInstance();
		scheduler->Reload();
		
		Tasks *tasks = Tasks::GetInstance();
		tasks->Reload();
		
		RetrySchedules *retry_schedules = RetrySchedules::GetInstance();
		retry_schedules->Reload();
		
		Workflows *workflows = Workflows::GetInstance();
		workflows->Reload();
	}
	else if(signum==SIGUSR1)
	{
		Logger::Log(LOG_NOTICE,"Got SIGUSR1, flushing retrier");
		Retrier *retrier = Retrier::GetInstance();
		retrier->Flush();
	}
}

int main(int argc,const char **argv)
{
	// Initialize external libraries
	mysql_library_init(0,0,0);
	XQillaPlatformUtils::initialize();
	
	openlog("evqueue",0,LOG_DAEMON);
	
	struct sigaction sa;
	sigset_t block_mask;
	
	sigemptyset(&block_mask);
	sigaddset(&block_mask, SIGINT);
	sigaddset(&block_mask, SIGTERM);
	sigaddset(&block_mask, SIGHUP);
	sigaddset(&block_mask, SIGUSR1);
	
	sa.sa_handler = signal_callback_handler;
	sa.sa_mask = block_mask;
	sa.sa_flags = 0;
	
	sigaction(SIGHUP,&sa,0);
	sigaction(SIGINT,&sa,0);
	sigaction(SIGTERM,&sa,0);
	sigaction(SIGUSR1,&sa,0);
	
	try
	{
		// Check if we have a configuration filename
		if(argc!=2)
			throw Exception("core","First parameter must be a configuration filename");
		
		// Read configuration
		Configuration *config;
		config = ConfigurationReader::Read(argv[1]);
		
		// Create logger as soon as possible
		Logger *logger = new Logger();
		
		int gid = atoi(config->Get("core.gid"));
		if(gid!=0 && setgid(gid)!=0)
			throw Exception("core","Unable to set requested GID");
		
		// Set uid/gid if requested
		int uid = atoi(config->Get("core.uid"));
		if(uid!=0 && setuid(uid)!=0)
			throw Exception("core","Unable to set requested UID");
		
		// Create statistics counter
		Statistics *stats = new Statistics();
		
		// Start retrier
		Retrier *retrier = new Retrier();
		
		// Start scheduler
		WorkflowScheduler *scheduler = new WorkflowScheduler();
		
		// Create queue pool
		QueuePool *pool = new QueuePool();
		
		// Instanciate workflow instances map
		WorkflowInstances *workflow_instances = new WorkflowInstances();
		
		// Instanciate workflows list
		Workflows *workflows = new Workflows();
		
		// Instanciate tasks list
		Tasks *tasks = new Tasks();
		
		// Instanciate retry schedules list
		RetrySchedules *retry_schedules = new RetrySchedules();
		
		// Check if workflows are to resume (we have to resume them before starting ProcessManager)
		DB db;
		db.Query("SELECT workflow_instance_id, workflow_schedule_id FROM t_workflow_instance WHERE workflow_instance_status='EXECUTING'");
		while(db.FetchRow())
		{
			Logger::Log(LOG_NOTICE,"[WID %d] Resuming",db.GetFieldInt(0));
			
			WorkflowInstance *workflow_instance = 0;
			bool workflow_terminated;
			try
			{
				workflow_instance = new WorkflowInstance(db.GetFieldInt(0));
				workflow_instance->Resume(&workflow_terminated);
				if(workflow_terminated)
					delete workflow_instance;
			}
			catch(Exception &e)
			{
				Logger::Log(LOG_NOTICE,"[WID %d] Unexpected exception trying to resume : [ %s ] %s\n",db.GetFieldInt(0),e.context,e.error);
				
				if(workflow_instance)
					delete workflow_instance;
			}
		}
		
		// Load workflow schedules
		db.Query("SELECT ws.workflow_schedule_id, w.workflow_name, wi.workflow_instance_id FROM t_workflow_schedule ws LEFT JOIN t_workflow_instance wi ON(wi.workflow_schedule_id=ws.workflow_schedule_id AND wi.workflow_instance_status='EXECUTING') INNER JOIN t_workflow w ON(ws.workflow_id=w.workflow_id) WHERE ws.workflow_schedule_active=1");
		while(db.FetchRow())
		{
			WorkflowSchedule *workflow_schedule = 0;
			try
			{
				workflow_schedule = new WorkflowSchedule(db.GetFieldInt(0));
				scheduler->ScheduleWorkflow(workflow_schedule, db.GetFieldInt(2));
			}
			catch(Exception &e)
			{
				Logger::Log(LOG_NOTICE,"[WSID %d] Unexpected exception trying initialize workflow schedule : [ %s ] %s\n",db.GetFieldInt(0),e.context,e.error);
				
				if(workflow_schedule)
					delete workflow_schedule;
			}
		}
		
		// Start Process Manager (Forker & Gatherer)
		ProcessManager *pm = new ProcessManager();
		
		// Start garbage GarbageCollector
		GarbageCollector *gc = new GarbageCollector();
		
		Logger::Log(LOG_NOTICE,"evqueue core started");
		
		int re,s,optval;
		struct sockaddr_in local_addr,remote_addr;
		socklen_t remote_addr_len;
		
		// Create listen socket
		listen_socket=socket(PF_INET,SOCK_STREAM,0);
		
		// Configure socket
		optval=1;
		setsockopt(listen_socket,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(int));
		
		// Bind socket
		memset(&local_addr,0,sizeof(struct sockaddr_in));
		local_addr.sin_family=AF_INET;
		if(strcmp(config->Get("network.bind.ip"),"*")==0)
			local_addr.sin_addr.s_addr=htonl(INADDR_ANY);
		else
			local_addr.sin_addr.s_addr=inet_addr(config->Get("network.bind.ip"));
		local_addr.sin_port = htons(atoi(config->Get("network.bind.port")));
		re=bind(listen_socket,(struct sockaddr *)&local_addr,sizeof(struct sockaddr_in));
		if(re==-1)
			throw Exception("core","Unable to bind listen socket");
		
		// Listen on socket
		re=listen(listen_socket,64);
		if(re==-1)
			throw Exception("core","Unable to listen on socket");
		
		char *ptr,*parameters;
		
		
		Logger::Log(LOG_NOTICE,"Accepting connection on port %s",config->Get("network.bind.port"));
		
		// Loop for incoming connections
		int len,*sp;
		while(1)
		{
			remote_addr_len=sizeof(struct sockaddr);
			s = accept(listen_socket,(struct sockaddr *)&remote_addr,&remote_addr_len);
			if(s<0)
			{
				if(errno==EINTR)
					continue; // Interrupted by signal, continue
				
				// Shutdown requested
				Logger::Log(LOG_NOTICE,"Shutting down...");
				
				// Request shutdown on ProcessManager and wait
				pm->Shutdown();
				pm->WaitForShutdown();
				
				// Request shutdown on scheduler and wait
				scheduler->Shutdown();
				scheduler->WaitForShutdown();
				
				// Request shutdown on Retrier and wait
				retrier->Shutdown();
				retrier->WaitForShutdown();
				
				// Save current state in database
				workflow_instances->RecordSavepoint();
				
				// Request shutdown on GarbageCollector and wait
				gc->Shutdown();
				gc->WaitForShutdown();
				
				// All threads have exited, we can cleanly exit
				Logger::Log(LOG_NOTICE,"Clean exit");
				return 0;
			}
			
			sp = new int;
			*sp = s;
			pthread_t thread;
			pthread_attr_t thread_attr;
			pthread_attr_init(&thread_attr);
			pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
			pthread_create(&thread, &thread_attr, handle_connection, sp);
		}
	}
	catch(Exception &e)
	{
		// We have to use only syslog here because the logger might not be instanciated yet
		syslog(LOG_CRIT,"Unexpected exception : [ %s ] %s\n",e.context,e.error);
		return -1;
	}
	
	return 0;
}
