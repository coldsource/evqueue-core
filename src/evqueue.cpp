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

#include <mysql/mysql.h>

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
#include <Notification/NotificationType.h>
#include <Notification/NotificationTypes.h>
#include <Notification/Notification.h>
#include <Notification/Notifications.h>
#include <User/User.h>
#include <User/Users.h>
#include <Tag/Tag.h>
#include <Tag/Tags.h>
#include <IO/Sockets.h>
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

#include <xercesc/util/PlatformUtils.hpp>

int listen_socket = -1;
int listen_socket_unix = -1;
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

void fork_parent_pre_handler(void)
{
	// We must fork with sockets locked to prevent operations on it during fork
	Sockets::GetInstance()->Lock();
}

void fork_parent_post_handler(void)
{
	// We entered the child in locked state, it is now safe to unlock
	Sockets::GetInstance()->Unlock();
}

void fork_child_handler(void)
{
	// We entered in locked state, unlock before following calls
	Sockets::GetInstance()->Unlock();
	
	if(listen_socket!=-1)
		close(listen_socket); // Close listen socket in child to allow process to restart when children are still running
	
	if(listen_socket_unix!=-1)
		close(listen_socket_unix); // Close listen socket in child to allow process to restart when children are still running
	
	Sockets::GetInstance()->CloseSockets(); // Close all open sockets to prevent hanged connections
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

int main(int argc,const char **argv)
{
	// Check parameters
	const char *config_filename = 0;
	bool daemonize = false;
	bool daemonized = false;
	
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
	
	// Initialize external libraries
	mysql_library_init(0,0,0);
	mysql_thread_init();
	
#ifdef USELIBGIT2
	git_libgit2_init();
#endif
	
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
	
	// Get start time for computing uptime
	time(&evqueue_start_time);
	
	try
	{
		// Read configuration
		ConfigurationEvQueue *config = new ConfigurationEvQueue();
		ConfigurationReader::Read(config_filename, config);
		
		// Substitute configuration variables with environment if needed
		config->Substitute();

		// Handle utils tasks if specified on command line. This must be done after configuration is loaded since QID is in configuration file
		if(ipcq_remove)
			return ipc_queue_destroy(ConfigurationEvQueue::GetInstance()->Get("core.ipc.qid").c_str());
		else if(ipcq_stats)
			return ipc_queue_stats(ConfigurationEvQueue::GetInstance()->Get("core.ipc.qid").c_str());
		else if(ipc_terminate_tid!=-1)
			return ipc_send_exit_msg(ConfigurationEvQueue::GetInstance()->Get("core.ipc.qid").c_str(),1,ipc_terminate_tid,-1);
		
		// Create logger as soon as possible
		Logger *logger = new Logger();
		
		// Create API logger
		LoggerAPI *logger_api = new LoggerAPI();
		
		// Create notifications logger
		LoggerNotifications *logger_notifications = new LoggerNotifications();
		
		// Set locale
		if(!setlocale(LC_ALL,config->Get("core.locale").c_str()))
		{
			Logger::Log(LOG_ERR,"Unknown locale : %s",config->Get("core.locale").c_str());
			throw Exception("core","Unable to set locale");
		}
		
		// Init Xerces after locale
		xercesc::XMLPlatformUtils::Initialize();
		
		// Get/Compute GID
		int gid;
		try
		{
			gid = std::stoi(config->Get("core.gid"));
		}
		catch(const std::invalid_argument& excpt)
		{
			struct group *group_entry = getgrnam(config->Get("core.gid").c_str());
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
			uid = std::stoi(config->Get("core.uid"));
		}
		catch(const std::invalid_argument& excpt)
		{
			struct passwd *user_entry = getpwnam(config->Get("core.uid").c_str());
			if(!user_entry)
				throw Exception("core","Unable to find user");
			
			uid = user_entry->pw_uid;
		}
		catch(const std::out_of_range & excpt)
		{
			throw Exception("core","Invalid UID");
		}
		
		// Change working directory
		if(config->Get("core.wd").length()>0)
		{
			if(chdir(config->Get("core.wd").c_str())!=0)
				throw Exception("core","Unable to change working directory");
		}

		// Sanity checks on configuration values and access rights
		config->Check();
		
		// Create directory for PID (usually in /var/run)
		char *pid_file2 = strdup(config->Get("core.pidfile").c_str());
		char *pid_directory = dirname(pid_file2);
		
		if(mkdir(pid_directory,0755)==0)
		{
			if(uid!=0)
			{
				if(chown(pid_directory,uid,-1)!=0)
					throw Exception("core","Unable to change pid file uid");
			}
			
			if(gid!=0)
			{
				if(chown(pid_directory,-1,gid)!=0)
					throw Exception("core","Unable to change pid file gid");
			}
		}
		
		free(pid_file2);
		
		// Set uid/gid if requested
		if(gid!=0 && setregid(gid,gid)!=0)
			throw Exception("core","Unable to set requested GID");
		
		if(uid!=0 && setreuid(uid,uid)!=0)
			throw Exception("core","Unable to set requested UID");
		
		// Open pid file before fork to eventually print errors
		FILE *pidfile = fopen(config->Get("core.pidfile").c_str(),"w");
		if(pidfile==0)
			throw Exception("core","Unable to open pid file");
		
		// Check database connection
		DB db;
		db.Ping();
		
		// Initialize DB
		tools_init_db();
		
		if(daemonize)
		{
			if(daemon(1,0)!=0)
				throw Exception("core","Error trying to daemonize process");
			
			daemonized = true;
		}
		
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
		db.QueryPrintf("SELECT workflow_instance_id, workflow_schedule_id FROM t_workflow_instance WHERE workflow_instance_status='EXECUTING' AND node_name=%s",&config->Get("cluster.node.name"));
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
				&config->Get("cluster.node.name"),
				&config->Get("cluster.node.name")
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
		qh->RegisterHandler("control",tools_handle_query);
		qh->RegisterHandler("status",tools_handle_query);
		qh->RegisterHandler("statistics",Statistics::HandleQuery);
		qh->RegisterHandler("ping",ping_handle_query);
		qh->RegisterHandler("git",Git::HandleQuery);
		qh->RegisterHandler("filesystem",Filesystem::HandleQuery);
		qh->RegisterHandler("datastore",Datastore::HandleQuery);
		qh->RegisterHandler("processmanager",ProcessManager::HandleQuery);
		
		// Create sockets set
		Sockets *sockets = new Sockets();
		pthread_atfork(fork_parent_pre_handler,fork_parent_post_handler,fork_child_handler);
		
		// Create active connections set
		ActiveConnections *active_connections = new ActiveConnections();
		
		// Initialize cluster
		Cluster *cluster = new Cluster();
		cluster->ParseConfiguration(config->Get("cluster.nodes"));
		
		Logger::Log(LOG_NOTICE,"evqueue core started");
		
		int re,s;
		struct sockaddr_un local_addr_unix,remote_addr_unix;
		socklen_t remote_addr_len_unix;
		
		struct sockaddr_in local_addr,remote_addr;
		socklen_t remote_addr_len;
		
		// Create TCP socket
		if(config->Get("network.bind.ip").length()>0)
		{
			int optval;
			
			// Create listen socket
			listen_socket=socket(PF_INET,SOCK_STREAM,0);
			
			// Configure socket
			optval=1;
			setsockopt(listen_socket,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(int));
			
			// Bind socket
			memset(&local_addr,0,sizeof(struct sockaddr_in));
			local_addr.sin_family=AF_INET;
			if(config->Get("network.bind.ip")=="*")
				local_addr.sin_addr.s_addr=htonl(INADDR_ANY);
			else
				local_addr.sin_addr.s_addr=inet_addr(config->Get("network.bind.ip").c_str());
			local_addr.sin_port = htons(config->GetInt("network.bind.port"));
			re=bind(listen_socket,(struct sockaddr *)&local_addr,sizeof(struct sockaddr_in));
			if(re==-1)
				throw Exception("core","Unable to bind listen socket");
			
			// Listen on socket
			re=listen(listen_socket,config->GetInt("network.listen.backlog"));
			if(re==-1)
				throw Exception("core","Unable to listen on socket");
			Logger::Log(LOG_NOTICE,"Listen backlog set to %d (tcp socket)",config->GetInt("network.listen.backlog"));
			
			Logger::Log(LOG_NOTICE,"Accepting connection on port %d",config->GetInt("network.bind.port"));
		}
		
		if(config->Get("network.bind.path").length()>0)
		{
			// Create UNIX socket
			listen_socket_unix=socket(AF_UNIX,SOCK_STREAM,0);
			
			// Bind socket
			local_addr_unix.sun_family=AF_UNIX;
			strcpy(local_addr_unix.sun_path,config->Get("network.bind.path").c_str());
			unlink(local_addr_unix.sun_path);
			re=bind(listen_socket_unix,(struct sockaddr *)&local_addr_unix,sizeof(struct sockaddr_un));
			if(re==-1)
				throw Exception("core","Unable to bind unix listen socket");
			
			// Listen on socket
			chmod(config->Get("network.bind.path").c_str(),0777);
			re=listen(listen_socket_unix,config->GetInt("network.listen.backlog"));
			if(re==-1)
				throw Exception("core","Unable to listen on unix socket");
			Logger::Log(LOG_NOTICE,"Listen backlog set to %d (unix socket)",config->GetInt("network.listen.backlog"));
			
			Logger::Log(LOG_NOTICE,"Accepting connection on unix socket %s",config->Get("network.bind.path").c_str());
		}
		
		if(listen_socket==-1 && listen_socket_unix==-1)
			throw Exception("core","You have to specify at least one listen socket");
		
		unsigned int max_conn = config->GetInt("network.connections.max");
		
		// Loop for incoming connections
		int len,*sp;
		fd_set rfds;
		while(1)
		{
			FD_ZERO(&rfds);
			
			if(listen_socket>=0)
				FD_SET(listen_socket,&rfds);
			
			if(listen_socket_unix>=0)
				FD_SET(listen_socket_unix,&rfds);
			
			re = select(MAX(listen_socket,listen_socket_unix)+1,&rfds,0,0,0);
			
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
				
				// Shutdown sockets to end active connections earlier
				if(config->GetBool("core.fastshutdown"))
				{
					Logger::Log(LOG_NOTICE,"Fast shutdown is enabled, shutting down sockets");
					sockets->ShutdownSockets();
				}
				
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
				delete sockets;
				delete cluster;
				delete users;
				delete tags;
				delete active_connections;
				delete random;
				delete logger_api;
				delete logger_notifications;
				
				xercesc::XMLPlatformUtils::Terminate();
				
				unlink(config->Get("core.pidfile").c_str());
				Logger::Log(LOG_NOTICE,"Clean exit");
				delete logger;
				delete config;
				
				mysql_thread_end();
				mysql_library_end();
				
#ifdef USELIBGIT2
				git_libgit2_shutdown();
#endif
				
				return 0;
			}
			
			if(listen_socket_unix>=0 && FD_ISSET(listen_socket_unix,&rfds))
			{
				// Got data on UNIX socket
				remote_addr_len_unix=sizeof(struct sockaddr);
				s = accept(listen_socket_unix,(struct sockaddr *)&remote_addr_unix,&remote_addr_len_unix);
			}
			else if(FD_ISSET(listen_socket,&rfds))
			{
				// Got data on TCP socket
				remote_addr_len=sizeof(struct sockaddr);
				s = accept(listen_socket,(struct sockaddr *)&remote_addr,&remote_addr_len);
			}
			
			if(s<0)
				continue; // We were interrupted or sockets were closed due to shutdown request, loop as select will also return an error
			
			// Check for max connections
			if(active_connections->GetNumber()==max_conn)
			{
				close(s);
				
				Logger::Log(LOG_WARNING,"Max connections reached, dropping connection");
				
				continue;
			}
			
			// Register socket so it can be closed on fork
			sockets->RegisterSocket(s);
			
			// Start thread
			active_connections->StartConnection(s);
		}
	}
	catch(Exception &e)
	{
		// We have to use only syslog here because the logger might not be instantiated yet
		syslog(LOG_CRIT,"Unexpected exception : [ %s ] %s\n",e.context.c_str(),e.error.c_str());
		
		if(!daemonized)
			fprintf(stderr,"Unexpected exception : [ %s ] %s\n",e.context.c_str(),e.error.c_str());
		
		if(ConfigurationEvQueue::GetInstance())
			unlink(ConfigurationEvQueue::GetInstance()->Get("core.pidfile").c_str());
		
		return -1;
	}
	
	return 0;
}
