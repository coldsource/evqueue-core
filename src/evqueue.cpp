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

#include <syslog.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>

#include <string>

#ifdef USELIBGIT2
#include <git2.h>
#endif

#include <Logger/Logger.h>
#include <Queue/Queue.h>
#include <Queue/QueuePool.h>
#include <Workflow/Workflow.h>
#include <Workflow/Workflows.h>
#include <WorkflowInstance/WorkflowInstance.h>
#include <WorkflowInstance/WorkflowInstances.h>
#include <Configuration/ConfigurationReader.h>
#include <Configuration/Configuration.h>
#include <Exception/Exception.h>
#include <Schedule/Retrier.h>
#include <Schedule/WorkflowScheduler.h>
#include <Schedule/WorkflowSchedule.h>
#include <Schedule/WorkflowSchedules.h>
#include <global.h>
#include <Process/ProcessManager.h>
#include <DB/DB.h>
#include <API/Statistics.h>
#include <Schedule/RetrySchedule.h>
#include <Schedule/RetrySchedules.h>
#include <DB/SequenceGenerator.h>
#include <Crypto/Random.h>
#include <Process/DataPiper.h>
#include <API/QueryHandlers.h>
#include <Cluster/Cluster.h>
#include <API/ActiveConnections.h>
#include <API/tools.h>
#include <Process/tools_ipc.h>
#include <DB/tools_db.h>
#include <WS/Events.h>
#include <WS/WSServer.h>
#include <Process/tools_proc.h>
#include <Process/Forker.h>
#include <IO/NetworkConnections.h>
#include <Process/PIDFile.h>
#include <Process/Args.h>

#include <xercesc/util/PlatformUtils.hpp>

time_t evqueue_start_time = 0;

void signal_callback_handler(int signum)
{
	if(signum==SIGINT || signum==SIGTERM)
	{
		// Shutdown requested
		Logger::Log(LOG_NOTICE,"Shutting down...");
		
		// Shutdown requested, close all listening sockets to release main loop
		if(NetworkConnections::GetInstance())
			NetworkConnections::GetInstance()->Shutdown();
		else
			exit(-1); // Server not yet started, just exit
	}
	else if(signum==SIGHUP)
	{
		Logger::Log(LOG_NOTICE,"Got SIGHUP, reloading configuration");
		tools_config_reload("all",false);
	}
	else if(signum==SIGUSR1)
	{
		tools_flush_retrier();
	}
}

void tools_print_usage()
{
	fprintf(stderr,"Usage :\n");
	fprintf(stderr,"  Launch evqueue      : evqueue (--daemon) --config <path to config file>\n");
	fprintf(stderr,"  Clean IPC queue     : evqueue --config <path to config file> --ipcq-remove\n");
	fprintf(stderr,"  Get IPC queue stats : evqueue --config <path to config file> --ipcq-stats\n");
	fprintf(stderr,"  Send IPC TERM-TID   : evqueue --config <path to config file> --ipc-terminate-tid <tid>\n");
	fprintf(stderr,"  Show version        : evqueue --version\n");
}

using namespace std;

int g_argc;
char **g_argv;

int main(int argc,char **argv)
{
	// Set global argc / argv to allow process name change
	g_argc = argc;
	g_argv = argv;
	
	const map<string, string> args_config = {
		{"--daemon", "flag"},
		{"--wait-db", "flag"},
		{"--config", "string"},
		{"--ipcq-remove", "flag"},
		{"--ipcq-stats", "flag"},
		{"--ipc-terminate-tid", "integer"},
		{"--version", "flag"}
	};
	
	Args args;
	try
	{
		args = Args(args_config, argc, argv);
	}
	catch(Exception &e)
	{
		printf("%s\n", e.error.c_str());
		tools_print_usage();
		return 0;
	}
	
	// Check parameters
	bool daemonized = false;
	
	if(args["--version"])
	{
		printf("evQueue version " EVQUEUE_VERSION " (built " __DATE__ ")\n");
		return 0;
	}
	
	string config_filename = args["--config"];
	if(config_filename=="")
	{
		tools_print_usage();
		return -1;
	}
	
	openlog("evqueue",0,LOG_DAEMON);
	
	// Get start time for computing uptime
	time(&evqueue_start_time);
	
	try
	{
		// Sanitize ENV
		sanitize_fds(3);
		
		// Read configuration
		Configuration *config = Configuration::GetInstance();
		config->Merge();
		
		ConfigurationReader::Read(config_filename, config);
		
		// Substitute configuration variables with environment if needed
		config->Substitute();
		
		// Handle utils tasks if specified on command line. This must be done after configuration is loaded since QID is in configuration file
		if(args["--ipcq-remove"])
			return ipc_queue_destroy(config->Get("core.ipc.qid").c_str());
		else if(args["--ipcq-stats"])
			return ipc_queue_stats(config->Get("core.ipc.qid").c_str());
		else if((int)args["--ipc-terminate-tid"]!=-1)
			return ipc_send_exit_msg(config->Get("core.ipc.qid").c_str(),1,args["--ipc-terminate-tid"],-1);
		
		// Get/Compute GID / UID
		int gid = config->GetGID("core.gid");
		int uid = config->GetUID("core.uid");
		
		// Change working directory
		if(config->Get("core.wd").length()>0)
		{
			if(chdir(config->Get("core.wd").c_str())!=0)
				throw Exception("core","Unable to change working directory");
		}
		
		// Set uid/gid if requested
		if(gid!=0 && setregid(gid,gid)!=0)
			throw Exception("core","Unable to set requested GID");
		
		if(uid!=0 && setreuid(uid,uid)!=0)
			throw Exception("core","Unable to set requested UID");
		
		// Start forker very early to get cleanest ENV as possible (we still need configuration so)
		Forker forker;
		forker.Start();
		
		// Position signal handlers
		set_sighandler(signal_callback_handler, {SIGINT, SIGTERM, SIGHUP, SIGUSR1});
		
		// Initialize external libraries
		DB::InitLibrary();
		DB::StartThread();
		
		// Create logger and events as soon as possible
		Logger logger;
		Events events;
		
		// Set locale
		if(!setlocale(LC_ALL,config->Get("core.locale").c_str()))
		{
			Logger::Log(LOG_ERR,"Unknown locale : %s",config->Get("core.locale").c_str());
			throw Exception("core","Unable to set locale");
		}
		
		// Sanity checks on configuration values and access rights
		config->Split();
		config->CheckAll();
		
		// Check database connection
		{
			DB db;
			if(args["--wait-db"])
				db.Wait();
			else
				db.Ping();
		}
		
		// Initialize DB
		tools_init_db();
		
		// Open pid file before fork to eventually print errors
		PIDFile pidf("core", config->Get("core.pidfile"));
		
		if(args["--daemon"])
		{
			if(daemon(1,0)!=0)
				throw Exception("core","Error trying to daemonize process");
			
			daemonized = true;
		}
		
		// Init forker (close pipes) after eventual fork made by daemon()
		forker.Init();
		
		// Init Xerces after locale and forker
		xercesc::XMLPlatformUtils::Initialize();
		
		// Write pid after daemonization
		pidf.Write(getpid());
		
		// Instanciate sequence generator, used for savepoint level 0 or 1
		SequenceGenerator seq;
		
		// Instanciate random numbers generator
		Random random;
		
#ifdef USELIBGIT2
		git_libgit2_init();
#endif
		
		// Create statistics counter
		Statistics stats;
		
		// Start retrier
		Retrier *retrier = new Retrier();
		
		// Start scheduler
		WorkflowSchedules *workflow_schedules = new WorkflowSchedules();
		WorkflowScheduler *scheduler = new WorkflowScheduler();
		
		// Create queue pool
		QueuePool *pool = new QueuePool();
		
		// Instanciate workflow instances map
		WorkflowInstances *workflow_instances = new WorkflowInstances();
		
		// Instanciate workflows list
		Workflows *workflows = new Workflows();
		
		// Instanciate retry schedules list
		RetrySchedules *retry_schedules = new RetrySchedules();
		
		// Check if workflows are to resume (we have to resume them before starting ProcessManager)
		workflow_instances->Resume();
		
		// Load workflow schedules
		scheduler->LoadDBState();
		
		// Start data piper before process manager
		DataPiper *dp = new DataPiper();
		
		// Start Process Manager (Forker & Gatherer)
		ProcessManager *pm = new ProcessManager();
		
		// Initialize general module structures
		QueryHandlers *qh = QueryHandlers::GetInstance();
		NetworkConnections nc;
		
		// Initialize query handlers
		qh->AutoInit();
		
		// Create active connections set
		ActiveConnections *active_connections = new ActiveConnections();
		
		// Initialize cluster
		Cluster cluster(config->Get("cluster.nodes"));
		
		Logger::Log(LOG_NOTICE,"evqueue core started");
		
		// Start Websocket server
		WSServer *ws = new WSServer();
		
		// Loop for incoming connections
		while(1)
		{
			if(!nc.select())
			{
				// End threads
				delete pm;
				delete dp;
				delete scheduler;
				delete retrier;
				delete ws;
				
				// Close all active connections
				delete active_connections;
				
				// Save current state in database
				workflow_instances->RecordSavepoint();
				
				// All threads have exited, we can cleanly exit
				delete workflow_schedules;
				delete pool;
				delete workflow_instances;
				delete workflows;
				delete retry_schedules;
				delete qh;
				
				xercesc::XMLPlatformUtils::Terminate();
				
				Logger::Log(LOG_NOTICE,"Clean exit");
				
				DB::StopThread();
				DB::FreeLibrary();
				
#ifdef USELIBGIT2
				git_libgit2_shutdown();
#endif
				
				// Lastly, delete config
				delete config;
				
				return 0;
			}
		}
	}
	catch(Exception &e)
	{
		// We have to use only syslog here because the logger might not be instantiated yet
		syslog(LOG_CRIT,"Unexpected exception in %s : %s\n",e.context.c_str(),e.error.c_str());
		
		if(!daemonized)
			fprintf(stderr,"Unexpected exception in %s : %s\n",e.context.c_str(),e.error.c_str());
		
		return -1;
	}
	
	return 0;
}
