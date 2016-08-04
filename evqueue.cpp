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

#include <string>
#include <stdexcept>

#include <mysql/mysql.h>

#include <Logger.h>
#include <Queue.h>
#include <QueuePool.h>
#include <Workflow.h>
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
#include <SequenceGenerator.h>
#include <Notifications.h>
#include <Sockets.h>
#include <QueryHandlers.h>
#include <handle_connection.h>
#include <tools.h>
#include <tools_db.h>

#include <xqilla/xqilla-dom3.hpp>

int listen_socket = -1;
int listen_socket_unix = -1;

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
		tools_config_reload();
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

int main(int argc,const char **argv)
{
	// Check parameters
	const char *config_filename = 0;
	bool daemonize = false;
	bool daemonized = false;
	
	bool ipcq_remove = false;
	bool ipcq_stats = false;
	
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
		// Read configuration
		Configuration *config;
		config = ConfigurationReader::Read(config_filename);
		
		// Substitute configuration variables with environment if needed
		config->Substitute();
		
		// Handle utils tasks if specified on command line. This must be done after configuration is loaded since QID is in configuration file
		if(ipcq_remove)
			return tools_queue_destroy();
		else if(ipcq_stats)
			return tools_queue_stats();
		
		// Create logger as soon as possible
		Logger *logger = new Logger();
		
		// Set locale
		if(!setlocale(LC_ALL,config->Get("core.locale").c_str()))
		{
			Logger::Log(LOG_ERR,"Unknown locale : %s",config->Get("core.locale").c_str());
			throw Exception("core","Unable to set locale");
		}
		
		// Init xQilla after locale
		XQillaPlatformUtils::initialize();
		
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
		
		// Create directory for PID (usually in /var/run)
		char *pid_file2 = strdup(config->Get("core.pidfile").c_str());
		char *pid_directory = dirname(pid_file2);
		mkdir(pid_directory,0755);
		
		if(uid!=0)
			chown(pid_directory,uid,-1);
		
		if(gid!=0)
			chown(pid_directory,-1,gid);
		
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
			daemon(1,0);
			daemonized = true;
		}
		
		// Write pid after daemonization
		fprintf(pidfile,"%d\n",getpid());
		fclose(pidfile);
		
		// Instanciate sequence generator, used for savepoint level 0 or 1
		SequenceGenerator *seq = new SequenceGenerator();
		
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
		
		// Instanciate notifications map
		Notifications *notifications = new Notifications();
		
		// Instanciate tasks list
		Tasks *tasks = new Tasks();
		
		// Instanciate retry schedules list
		RetrySchedules *retry_schedules = new RetrySchedules();
		
		// Check if workflows are to resume (we have to resume them before starting ProcessManager)
		db.QueryPrintf("SELECT workflow_instance_id, workflow_schedule_id FROM t_workflow_instance WHERE workflow_instance_status='EXECUTING' AND node_name=%s",config->Get("network.node.name").c_str());
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
		
		// On level 0 or 1, executing workflows are only stored during engine restart. Purge them since they are resumed
		if(Configuration::GetInstance()->GetInt("workflowinstance.savepoint.level")<=1)
			db.Query("DELETE FROM t_workflow_instance WHERE workflow_instance_status='EXECUTING'");
		
		// Load workflow schedules
		db.QueryPrintf("SELECT ws.workflow_schedule_id, w.workflow_name, wi.workflow_instance_id FROM t_workflow_schedule ws LEFT JOIN t_workflow_instance wi ON(wi.workflow_schedule_id=ws.workflow_schedule_id AND wi.workflow_instance_status='EXECUTING' AND wi.node_name=%s) INNER JOIN t_workflow w ON(ws.workflow_id=w.workflow_id) WHERE ws.node_name=%s AND ws.workflow_schedule_active=1",
				config->Get("network.node.name").c_str(),
				config->Get("network.node.name").c_str()
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
				Logger::Log(LOG_NOTICE,"[WSID %d] Unexpected exception trying initialize workflow schedule : [ %s ] %s\n",db.GetFieldInt(0),e.context,e.error);
				
				if(workflow_schedule)
					delete workflow_schedule;
			}
		}
		
		// Release database connection
		db.Disconnect();
		
		// Start Process Manager (Forker & Gatherer)
		ProcessManager *pm = new ProcessManager();
		
		// Start garbage GarbageCollector
		GarbageCollector *gc = new GarbageCollector();
		
		// Initialize query handlers
		QueryHandlers *qh = new QueryHandlers();
		qh->RegisterHandler("workflow",Workflow::HandleQuery);
		qh->RegisterHandler("workflows",Workflows::HandleQuery);
		
		// Create sockets set
		Sockets *sockets = new Sockets();
		pthread_atfork(fork_parent_pre_handler,fork_parent_post_handler,fork_child_handler);
		
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
				delete stats;
				delete retrier;
				delete scheduler;
				delete pool;
				delete workflow_instances;
				delete workflows;
				delete notifications;
				delete tasks;
				delete retry_schedules;
				delete pm;
				delete gc;
				delete seq;
				delete qh;
				delete sockets;
				
				XQillaPlatformUtils::terminate();
				
				unlink(config->Get("core.pidfile").c_str());
				Logger::Log(LOG_NOTICE,"Clean exit");
				delete logger;
				delete config;
				
				mysql_thread_end();
				mysql_library_end();
				
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
			if(sockets->GetNumber()==max_conn)
			{
				close(s);
				
				Logger::Log(LOG_WARNING,"Max connections reached, dropping connection");
				
				continue;
			}
			
			// Register socket so it can be closed on fork
			sockets->RegisterSocket(s);
			
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
		
		if(!daemonized)
			fprintf(stderr,"Unexpected exception : [ %s ] %s\n",e.context,e.error);
		
		if(Configuration::GetInstance())
			unlink(Configuration::GetInstance()->Get("core.pidfile").c_str());
		
		return -1;
	}
	
	return 0;
}
