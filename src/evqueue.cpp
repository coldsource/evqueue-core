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
#include <sys/un.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <errno.h>
#include <stdio.h>
#include <pwd.h>
#include <grp.h>
#include <libgen.h>
#include <time.h>

#include <string>
#include <stdexcept>


#ifdef USELIBGIT2
#include <git2.h>
#endif

#include <Logger/Logger.h>
#include <Logger/LoggerAPI.h>
#include <Logger/LoggerNotifications.h>
#include <Logs/Logs.h>
#include <Logs/LogsAPI.h>
#include <Logs/LogsNotifications.h>
#include <Queue/Queue.h>
#include <Queue/QueuePool.h>
#include <Workflow/Workflow.h>
#include <Workflow/Workflows.h>
#include <WorkflowInstance/WorkflowInstance.h>
#include <WorkflowInstance/WorkflowInstances.h>
#include <WorkflowInstance/WorkflowInstanceAPI.h>
#include <Configuration/ConfigurationReader.h>
#include <Configuration/ConfigurationEvQueue.h>
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
#include <DB/GarbageCollector.h>
#include <DB/SequenceGenerator.h>
#include <Crypto/Random.h>
#include <Process/DataPiper.h>
#include <WorkflowInstance/Datastore.h>
#include <API/QueryHandlers.h>
#include <Cluster/Cluster.h>
#include <API/ActiveConnections.h>
#include <Git/Git.h>
#include <IO/Filesystem.h>
#include <API/handle_connection.h>
#include <API/tools.h>
#include <Process/tools_ipc.h>
#include <DB/tools_db.h>
#include <API/ping.h>
#include <XPath/XPathAPI.h>
#include <WS/Events.h>
#include <WS/WSServer.h>
#include <Process/tools_proc.h>
#include <Process/Forker.h>
#include <IO/NetworkConnections.h>

#include <xercesc/util/PlatformUtils.hpp>

#define CNX_TYPE_API    1
#define CNX_TYPE_WS     2
#define CNX_TYPE_ELOG   3

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
	
	// Check parameters
	const char *config_filename = 0;
	bool daemonize = false;
	bool daemonized = false;
	
	bool wait_db = false;
	
	bool ipcq_remove = false;
	bool ipcq_stats = false;
	int ipc_terminate_tid = -1;
	
	for(int i=1;i<argc;i++)
	{
		if(strcmp(argv[i],"--daemon")==0)
			daemonize = true;
		else if(strcmp(argv[i],"--config")==0 && i+1<argc)
		{
			config_filename = argv[i+1];
			i++;
		}
		else if(strcmp(argv[i],"--wait-db")==0)
		{
			wait_db = true;
			break;
		}
		else if(strcmp(argv[i],"--ipcq-remove")==0)
		{
			ipcq_remove = true;
			break;
		}
		else if(strcmp(argv[i],"--ipcq-stats")==0)
		{
			ipcq_stats = true;
			break;
		}
		else if(strcmp(argv[i],"--ipc-terminate-tid")==0 && i+1<argc)
		{
			ipc_terminate_tid = atoi(argv[i+1]);
			break;
		}
		else if(strcmp(argv[i],"--version")==0)
		{
			printf("evQueue version " EVQUEUE_VERSION " (built " __DATE__ ")\n");
			return 0;
		}
		else
		{
			fprintf(stderr,"Unknown option : %s\n",argv[i]);
			tools_print_usage();
			return -1;
		}
	}
	
	if(config_filename==0)
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
		ConfigurationEvQueue config;
		ConfigurationReader::Read(config_filename, &config);
		
		// Substitute configuration variables with environment if needed
		config.Substitute();
		
		// Handle utils tasks if specified on command line. This must be done after configuration is loaded since QID is in configuration file
		if(ipcq_remove)
			return ipc_queue_destroy(ConfigurationEvQueue::GetInstance()->Get("core.ipc.qid").c_str());
		else if(ipcq_stats)
			return ipc_queue_stats(ConfigurationEvQueue::GetInstance()->Get("core.ipc.qid").c_str());
		else if(ipc_terminate_tid!=-1)
			return ipc_send_exit_msg(ConfigurationEvQueue::GetInstance()->Get("core.ipc.qid").c_str(),1,ipc_terminate_tid,-1);
		
		// Get/Compute GID
		int gid;
		try
		{
			gid = std::stoi(config.Get("core.gid"));
		}
		catch(const std::invalid_argument& excpt)
		{
			struct group *group_entry = getgrnam(config.Get("core.gid").c_str());
			if(!group_entry)
				throw Exception("core","Unable to find group");
			
			gid = group_entry->gr_gid;
		}
		catch(const std::out_of_range & excpt)
		{
			throw Exception("core","Invalid GID");
		}
		
		// Get/Compute UID
		int uid;
		try
		{
			uid = std::stoi(config.Get("core.uid"));
		}
		catch(const std::invalid_argument& excpt)
		{
			struct passwd *user_entry = getpwnam(config.Get("core.uid").c_str());
			if(!user_entry)
				throw Exception("core","Unable to find user");
			
			uid = user_entry->pw_uid;
		}
		catch(const std::out_of_range & excpt)
		{
			throw Exception("core","Invalid UID");
		}
		
		// Change working directory
		if(config.Get("core.wd").length()>0)
		{
			if(chdir(config.Get("core.wd").c_str())!=0)
				throw Exception("core","Unable to change working directory");
		}
		
		// Set uid/gid if requested
		if(gid!=0 && setregid(gid,gid)!=0)
			throw Exception("core","Unable to set requested GID");
		
		if(uid!=0 && setreuid(uid,uid)!=0)
			throw Exception("core","Unable to set requested UID");
		
		// Start forker very early to get cleanest ENV as possible (we still need configuration so)
		Forker forker;
		pid_t forker_pid = forker.Start();
		if(forker_pid==0)
		{
			unlink(config.Get("forker.pidfile").c_str());
			return 0; // Forker clean exit
		}
		
		if(forker_pid<0)
			throw Exception("core", "Could not start forker, fork() returned error");
		
		// Open pid file before fork to eventually print errors
		FILE *forker_pidfile = fopen(config.Get("forker.pidfile").c_str(),"w");
		if(forker_pidfile==0)
			throw Exception("core","Unable to open forker pid file");
		
		fprintf(forker_pidfile,"%d\n",forker_pid);
		fclose(forker_pidfile);
		
		// Position signal handlers
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
		
		// Initialize external libraries
		DB::InitLibrary();
		DB::StartThread();
		
#ifdef USELIBGIT2
		git_libgit2_init();
#endif
		
		// Create logger and events as soon as possible
		Logger *logger = new Logger();
		Events *events = new Events();
		
		// Create API logger
		LoggerAPI *logger_api = new LoggerAPI();
		
		// Create notifications logger
		LoggerNotifications *logger_notifications = new LoggerNotifications();
		
		// Set locale
		if(!setlocale(LC_ALL,config.Get("core.locale").c_str()))
		{
			Logger::Log(LOG_ERR,"Unknown locale : %s",config.Get("core.locale").c_str());
			throw Exception("core","Unable to set locale");
		}
		
		// Init Xerces after locale
		xercesc::XMLPlatformUtils::Initialize();
		
		// Sanity checks on configuration values and access rights
		config.Check();
		
		// Check database connection
		DB db;
		while(true)
		{
			try
			{
				db.Ping();
			}
			catch(Exception &e)
			{
				if(wait_db && (e.codeno==2002 || e.codeno==2013))
				{
					Logger::Log(LOG_WARNING, "Database not yet ready, retrying...");
					sleep(1);
					continue;
				}
				
				throw e;
			}
			
			break;
		}
		
		// Initialize DB
		tools_init_db();
		
		// Open pid file before fork to eventually print errors
		FILE *pidfile = fopen(config.Get("core.pidfile").c_str(),"w");
		if(pidfile==0)
			throw Exception("core","Unable to open pid file");
		
		if(daemonize)
		{
			if(daemon(1,0)!=0)
				throw Exception("core","Error trying to daemonize process");
			
			daemonized = true;
		}
		
		// Init forker (close pipes) after eventual fork made by daemon()
		forker.Init();
		
		// Write pid after daemonization
		fprintf(pidfile,"%d\n",getpid());
		fclose(pidfile);
		
		// Instanciate sequence generator, used for savepoint level 0 or 1
		SequenceGenerator *seq = new SequenceGenerator();
		
		// Instanciate random numbers generator
		Random *random = new Random();
		
		// Git repository
		Git *git = new Git();
		
		// Create statistics counter
		Statistics *stats = new Statistics();
		
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
		db.QueryPrintf("SELECT workflow_instance_id, workflow_schedule_id FROM t_workflow_instance WHERE workflow_instance_status='EXECUTING' AND node_name=%s",&config.Get("cluster.node.name"));
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
				Logger::Log(LOG_NOTICE,"[WID %d] Unexpected exception trying to resume : [ %s ] %s\n",db.GetFieldInt(0),e.context.c_str(),e.error.c_str());
				
				if(workflow_instance)
					delete workflow_instance;
			}
		}
		
		// On level 0 or 1, executing workflows are only stored during engine restart. Purge them since they are resumed
		if(ConfigurationEvQueue::GetInstance()->GetInt("workflowinstance.savepoint.level")<=1)
			db.Query("DELETE FROM t_workflow_instance WHERE workflow_instance_status='EXECUTING'");
		
		// Load workflow schedules
		db.QueryPrintf("SELECT ws.workflow_schedule_id, w.workflow_name, wi.workflow_instance_id FROM t_workflow_schedule ws LEFT JOIN t_workflow_instance wi ON(wi.workflow_schedule_id=ws.workflow_schedule_id AND wi.workflow_instance_status='EXECUTING' AND wi.node_name=%s) INNER JOIN t_workflow w ON(ws.workflow_id=w.workflow_id) WHERE ws.node_name IN(%s,'all','any') AND ws.workflow_schedule_active=1",
				&config.Get("cluster.node.name"),
				&config.Get("cluster.node.name")
			);
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
				Logger::Log(LOG_NOTICE,"[WSID %d] Unexpected exception trying initialize workflow schedule : [ %s ] %s\n",db.GetFieldInt(0),e.context.c_str(),e.error.c_str());
				
				if(workflow_schedule)
					delete workflow_schedule;
			}
		}
		
		// Release database connection
		db.Disconnect();
		
		// Start data piper before process manager
		DataPiper *dp = new DataPiper();
		
		// Start Process Manager (Forker & Gatherer)
		ProcessManager *pm = new ProcessManager();
		
		// Start garbage GarbageCollector
		GarbageCollector *gc = new GarbageCollector();
		
		// Initialize general module structures
		QueryHandlers *qh = QueryHandlers::GetInstance();
		NetworkConnections nc;
		
		// Initialize query handlers
		qh->RegisterHandler("workflow",Workflow::HandleQuery);
		qh->RegisterHandler("workflows",Workflows::HandleQuery);
		qh->RegisterHandler("instance",WorkflowInstanceAPI::HandleQuery);
		qh->RegisterHandler("instances",WorkflowInstances::HandleQuery);
		qh->RegisterHandler("queue",Queue::HandleQuery);
		qh->RegisterHandler("queuepool",QueuePool::HandleQuery);
		qh->RegisterHandler("retry_schedule",RetrySchedule::HandleQuery);
		qh->RegisterHandler("retry_schedules",RetrySchedules::HandleQuery);
		qh->RegisterHandler("workflow_schedule",WorkflowSchedule::HandleQuery);
		qh->RegisterHandler("workflow_schedules",WorkflowSchedules::HandleQuery);
		qh->RegisterHandler("logs",Logs::HandleQuery);
		qh->RegisterHandler("logsapi",LogsAPI::HandleQuery);
		qh->RegisterHandler("logsnotifications",LogsNotifications::HandleQuery);
		qh->RegisterHandler("control",tools_handle_query);
		qh->RegisterHandler("status",tools_handle_query);
		qh->RegisterHandler("statistics",Statistics::HandleQuery);
		qh->RegisterHandler("ping",ping_handle_query);
		qh->RegisterHandler("git",Git::HandleQuery);
		qh->RegisterHandler("filesystem",Filesystem::HandleQuery);
		qh->RegisterHandler("datastore",Datastore::HandleQuery);
		qh->RegisterHandler("processmanager",ProcessManager::HandleQuery);
		qh->RegisterHandler("xpath",XPathAPI::HandleQuery);
		qh->AutoInit();
		
		// Create active connections set
		ActiveConnections *active_connections = new ActiveConnections();
		
		// Initialize cluster
		Cluster *cluster = new Cluster();
		cluster->ParseConfiguration(config.Get("cluster.nodes"));
		
		Logger::Log(LOG_NOTICE,"evqueue core started");
		
		int re,s;
		struct sockaddr_un local_addr_unix,remote_addr_unix;
		socklen_t remote_addr_len_unix;
		
		struct sockaddr_in local_addr,remote_addr;
		socklen_t remote_addr_len;
		
		// Create TCP and UNIX sockets
		NetworkConnections::t_stream_handler api_handler = [](int s) {
			if(ActiveConnections::GetInstance()->GetAPINumber()>=ConfigurationEvQueue::GetInstance()->GetInt("network.connections.max"))
			{
				close(s);
				
				Logger::Log(LOG_WARNING,"Max API connections reached, dropping connection");
			}
			
			ActiveConnections::GetInstance()->StartAPIConnection(s);
		};
		
		nc.RegisterTCP("API (tcp)", config.Get("network.bind.ip"), config.GetInt("network.bind.port"), config.GetInt("network.listen.backlog"), api_handler);
		nc.RegisterUNIX("API (unix)", config.Get("network.bind.path"), config.GetInt("network.listen.backlog"), api_handler);
		
		// Create Websocket TCP socket
		nc.RegisterTCP("WebSocket (tcp)", config.Get("ws.bind.ip"), config.GetInt("ws.bind.port"), config.GetInt("ws.listen.backlog"), [](int s) {
			if(ActiveConnections::GetInstance()->GetWSNumber()>=ConfigurationEvQueue::GetInstance()->GetInt("ws.connections.max"))
			{
				close(s);
				
				Logger::Log(LOG_WARNING,"Max WebSocket connections reached, dropping connection");
			}
			
			ActiveConnections::GetInstance()->StartWSConnection(s);
		});
		
		WSServer *ws = new WSServer();
		
		// Loop for incoming connections
		while(1)
		{
			if(!nc.select())
			{
				// Request shutdown on ProcessManager and wait
				pm->Shutdown();
				pm->WaitForShutdown();
				
				// Request shutdown on data piper and wait
				dp->Shutdown();
				dp->WaitForShutdown();
				
				// Request shutdown on scheduler and wait
				scheduler->Shutdown();
				scheduler->WaitForShutdown();
				
				// Request shutdown on Retrier and wait
				retrier->Shutdown();
				retrier->WaitForShutdown();
				
				// Request shutdown on websockets server and wait
				ws->Shutdown();
				ws->WaitForShutdown();
				
				// Wait for active connections to end
				Logger::Log(LOG_NOTICE,"Waiting for active connections to end...");
				active_connections->Shutdown();
				active_connections->WaitForShutdown();
				
				// Save current state in database
				workflow_instances->RecordSavepoint();
				
				// All threads have exited, we can cleanly exit
				delete stats;
				delete git;
				delete retrier;
				delete scheduler;
				delete workflow_schedules;
				delete pool;
				delete workflow_instances;
				delete workflows;
				delete retry_schedules;
				delete pm;
				delete dp;
				delete gc;
				delete seq;
				delete qh;
				delete cluster;
				delete active_connections;
				delete random;
				delete logger_api;
				delete logger_notifications;
				delete events;
				delete ws;
				
				xercesc::XMLPlatformUtils::Terminate();
				
				unlink(config.Get("core.pidfile").c_str());
				if(config.Get("network.bind.path").length()>0)
					unlink(config.Get("network.bind.path").c_str());
				Logger::Log(LOG_NOTICE,"Clean exit");
				delete logger;
				
				DB::StopThread();
				DB::FreeLibrary();
				
#ifdef USELIBGIT2
				git_libgit2_shutdown();
#endif
				
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
		
		if(ConfigurationEvQueue::GetInstance())
			unlink(ConfigurationEvQueue::GetInstance()->Get("core.pidfile").c_str());
		
		return -1;
	}
	
	return 0;
}
