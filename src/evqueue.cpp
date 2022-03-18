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
#include <Logs/Channel.h>
#include <Logs/Channels.h>
#include <Logs/LogStorage.h>
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
#include <Notification/NotificationType.h>
#include <Notification/NotificationTypes.h>
#include <Notification/Notification.h>
#include <Notification/Notifications.h>
#include <User/User.h>
#include <User/Users.h>
#include <Tag/Tag.h>
#include <Tag/Tags.h>
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

#include <xercesc/util/PlatformUtils.hpp>

#define CNX_TYPE_API 1
#define CNX_TYPE_WS 2

int listen_socket = -1;
int listen_socket_unix = -1;
int listen_socket_ws = -1;
time_t evqueue_start_time = 0;

void signal_callback_handler(int signum)
{
	if(signum==SIGINT || signum==SIGTERM)
	{
		// Shutdown requested
		// Close main listen socket, this will release accept() loop
		if(listen_socket!=-1)
			close(listen_socket);
		
		if(listen_socket_unix!=-1)
			close(listen_socket_unix);
		
		if(listen_socket==-1 && listen_socket_unix==-1 && listen_socket_unix==-1)
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
		
		// Instanciate notifications map
		NotificationTypes *notification_types = new NotificationTypes();
		Notifications *notifications = new Notifications();
		
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
		
		// Load users
		Users *users = new Users();
		User::InitAnonymous();
		
		// Load tags
		Tags *tags = new Tags();
		
		// Load channels and logs storage
		Channels *channels = new Channels();
		LogStorage *log_storage = new LogStorage();
		
		// Initialize query handlers
		QueryHandlers *qh = new QueryHandlers();
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
		qh->RegisterHandler("notification_type",NotificationType::HandleQuery);
		qh->RegisterHandler("notification_types",NotificationTypes::HandleQuery);
		qh->RegisterHandler("notification",Notification::HandleQuery);
		qh->RegisterHandler("notifications",Notifications::HandleQuery);
		qh->RegisterHandler("user",User::HandleQuery);
		qh->RegisterHandler("users",Users::HandleQuery);
		qh->RegisterHandler("tag",Tag::HandleQuery);
		qh->RegisterHandler("tags",Tags::HandleQuery);
		qh->RegisterHandler("logs",Logs::HandleQuery);
		qh->RegisterHandler("logsapi",LogsAPI::HandleQuery);
		qh->RegisterHandler("logsnotifications",LogsNotifications::HandleQuery);
		qh->RegisterHandler("channel",Channel::HandleQuery);
		qh->RegisterHandler("channels",Channels::HandleQuery);
		qh->RegisterHandler("control",tools_handle_query);
		qh->RegisterHandler("status",tools_handle_query);
		qh->RegisterHandler("statistics",Statistics::HandleQuery);
		qh->RegisterHandler("ping",ping_handle_query);
		qh->RegisterHandler("git",Git::HandleQuery);
		qh->RegisterHandler("filesystem",Filesystem::HandleQuery);
		qh->RegisterHandler("datastore",Datastore::HandleQuery);
		qh->RegisterHandler("processmanager",ProcessManager::HandleQuery);
		qh->RegisterHandler("xpath",XPathAPI::HandleQuery);
		
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
		
		// Create TCP socket
		if(config.Get("network.bind.ip").length()>0)
		{
			int optval;
			
			// Create listen socket
			listen_socket=socket(PF_INET,SOCK_STREAM | SOCK_CLOEXEC,0);
			
			// Configure socket
			optval=1;
			setsockopt(listen_socket,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(int));
			
			// Bind socket
			memset(&local_addr,0,sizeof(struct sockaddr_in));
			local_addr.sin_family=AF_INET;
			if(config.Get("network.bind.ip")=="*")
				local_addr.sin_addr.s_addr=htonl(INADDR_ANY);
			else
				local_addr.sin_addr.s_addr=inet_addr(config.Get("network.bind.ip").c_str());
			local_addr.sin_port = htons(config.GetInt("network.bind.port"));
			re=bind(listen_socket,(struct sockaddr *)&local_addr,sizeof(struct sockaddr_in));
			if(re==-1)
				throw Exception("core","Unable to bind API listen socket");
			
			// Listen on socket
			re=listen(listen_socket,config.GetInt("network.listen.backlog"));
			if(re==-1)
				throw Exception("core","Unable to listen on API socket");
			Logger::Log(LOG_NOTICE,"API listen backlog set to %d (tcp socket)",config.GetInt("network.listen.backlog"));
			
			Logger::Log(LOG_NOTICE,"API accepting connections on port %d",config.GetInt("network.bind.port"));
		}
		
		if(config.Get("network.bind.path").length()>0)
		{
			// Create UNIX socket
			listen_socket_unix=socket(AF_UNIX,SOCK_STREAM | SOCK_CLOEXEC,0);
			
			// Bind socket
			local_addr_unix.sun_family=AF_UNIX;
			strcpy(local_addr_unix.sun_path,config.Get("network.bind.path").c_str());
			unlink(local_addr_unix.sun_path);
			re=bind(listen_socket_unix,(struct sockaddr *)&local_addr_unix,sizeof(struct sockaddr_un));
			if(re==-1)
				throw Exception("core","Unable to bind API unix listen socket");
			
			// Listen on socket
			chmod(config.Get("network.bind.path").c_str(),0777);
			re=listen(listen_socket_unix,config.GetInt("network.listen.backlog"));
			if(re==-1)
				throw Exception("core","Unable to listen on API unix socket");
			Logger::Log(LOG_NOTICE,"API listen backlog set to %d (unix socket)",config.GetInt("network.listen.backlog"));
			
			Logger::Log(LOG_NOTICE,"API accepting connections on unix socket %s",config.Get("network.bind.path").c_str());
		}
		
		if(config.Get("ws.bind.ip").length()>0)
		{
			int optval;
			
			// Create listen socket
			listen_socket_ws=socket(PF_INET,SOCK_STREAM | SOCK_CLOEXEC,0);
			
			// Configure socket
			optval=1;
			setsockopt(listen_socket_ws,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(int));
			
			// Bind socket
			memset(&local_addr,0,sizeof(struct sockaddr_in));
			local_addr.sin_family=AF_INET;
			if(config.Get("ws.bind.ip")=="*")
				local_addr.sin_addr.s_addr=htonl(INADDR_ANY);
			else
				local_addr.sin_addr.s_addr=inet_addr(config.Get("ws.bind.ip").c_str());
			local_addr.sin_port = htons(config.GetInt("ws.bind.port"));
			re=bind(listen_socket_ws,(struct sockaddr *)&local_addr,sizeof(struct sockaddr_in));
			if(re==-1)
				throw Exception("core","Unable to bind WS listen socket");
			
			// Listen on socket
			re=listen(listen_socket_ws,config.GetInt("ws.listen.backlog"));
			if(re==-1)
				throw Exception("core","Unable to listen on WS socket");
			Logger::Log(LOG_NOTICE,"WS listen backlog set to %d (tcp socket)",config.GetInt("ws.listen.backlog"));
			
			Logger::Log(LOG_NOTICE,"WS accepting connections on port %d",config.GetInt("ws.bind.port"));
		}
		
		if(listen_socket==-1 && listen_socket_unix==-1)
			throw Exception("core","You have to specify at least one listen socket");
		
		unsigned int max_conn_api = config.GetInt("network.connections.max");
		unsigned int max_conn_ws = config.GetInt("ws.connections.max");
		
		WSServer *ws = new WSServer();
		
		// Loop for incoming connections
		int cnx_type;
		int len,*sp;
		fd_set rfds;
		while(1)
		{
			FD_ZERO(&rfds);
			int fdmax = -1;
			
			if(listen_socket>=0)
			{
				FD_SET(listen_socket,&rfds);
				fdmax = MAX(fdmax,listen_socket);
			}
			
			if(listen_socket_unix>=0)
			{
				FD_SET(listen_socket_unix,&rfds);
				fdmax = MAX(fdmax,listen_socket_unix);
			}
			
			if(listen_socket_ws>=0)
			{
				FD_SET(listen_socket_ws,&rfds);
				fdmax = MAX(fdmax,listen_socket_ws);
			}
			
			re = select(fdmax+1,&rfds,0,0,0);
			
			if(re<0)
			{
				if(errno==EINTR)
					continue; // Interrupted by signal, continue
				
				// Shutdown requested
				Logger::Log(LOG_NOTICE,"Shutting down...");
				
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
				
				// Request shutdown on GarbageCollector and wait
				gc->Shutdown();
				gc->WaitForShutdown();
				
				// All threads have exited, we can cleanly exit
				delete stats;
				delete git;
				delete retrier;
				delete scheduler;
				delete workflow_schedules;
				delete pool;
				delete workflow_instances;
				delete workflows;
				delete notifications;
				delete notification_types;
				delete retry_schedules;
				delete pm;
				delete dp;
				delete gc;
				delete seq;
				delete qh;
				delete cluster;
				delete users;
				delete tags;
				delete channels;
				delete log_storage;
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
			
			if(listen_socket_unix>=0 && FD_ISSET(listen_socket_unix,&rfds))
			{
				// Got data on UNIX socket
				remote_addr_len_unix=sizeof(struct sockaddr);
				s = accept4(listen_socket_unix,(struct sockaddr *)&remote_addr_unix,&remote_addr_len_unix,SOCK_CLOEXEC);
				cnx_type = CNX_TYPE_API;
			}
			else if(FD_ISSET(listen_socket,&rfds))
			{
				// Got data on TCP socket
				remote_addr_len=sizeof(struct sockaddr);
				s = accept4(listen_socket,(struct sockaddr *)&remote_addr,&remote_addr_len,SOCK_CLOEXEC);
				cnx_type = CNX_TYPE_API;
			}
			else if(FD_ISSET(listen_socket_ws,&rfds))
			{
				// Got data on TCP socket
				remote_addr_len=sizeof(struct sockaddr);
				s = accept4(listen_socket_ws,(struct sockaddr *)&remote_addr,&remote_addr_len,SOCK_CLOEXEC);
				cnx_type = CNX_TYPE_WS;
			}
			
			if(s<0)
				continue; // We were interrupted or sockets were closed due to shutdown request, loop as select will also return an error
			
			// Check for max connections
			if(cnx_type==CNX_TYPE_API && active_connections->GetAPINumber()==max_conn_api)
			{
				close(s);
				
				Logger::Log(LOG_WARNING,"Max API connections reached, dropping connection");
				
				continue;
			}
			
			if(cnx_type==CNX_TYPE_WS && active_connections->GetWSNumber()==max_conn_ws)
			{
				close(s);
				
				Logger::Log(LOG_WARNING,"Max WS connections reached, dropping connection");
				
				continue;
			}
			
			if(cnx_type==CNX_TYPE_API)
			{
				// Start thread
				active_connections->StartAPIConnection(s);
			}
			
			if(cnx_type==CNX_TYPE_WS)
			{
				// Transfer the socket to LWS
				active_connections->StartWSConnection(s);
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
