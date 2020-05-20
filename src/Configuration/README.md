# evQueue configuration

## core

evQueue core process configuration

### core.gid (numeric) : 0

### core.uid (numeric) : 0

Numeric or text UID and GID of the process. 0 means to keep current user. If you want the process to change user or group, you must run evqueue as root.

### core.wd (string) : Ø

The working directory of the daemon.

### core.pidfile (string) : /tmp/evqueue.pid

Filename of the pidfile created by evqueue at startup. If you run evQueue as a daemon, you probably want to set this to **/run/evqueue/evqueue.pid**. See **evqueue-core.evqueue.service** file in debian directory.

### core.ipc.qid (string) : 0xEA023E3C

evQueue uses IPC messages to communicate between monitors and the core daemon. The queue is identified by an integer that must be unique on the machine. If you run multiple nodes on the same machine, you MUST change this value on each running instance.

You can also specify a path to a directory (which must exist) that will be used to generate a unique name.

### core.locale (string) : C.UTF-8

The locale that will be used by DOM to parse strings. The default locale should suit most cases.


### core.auth.enable (boolean) : yes

This is used to disable authentication. This will disable authentication on the core and also on the web interface.

### core.fastshutdown (boolean) : yes

Close existing connections when exit is required. Client that are waiting for instances termination will be disconnected.

Take care if you disable this feature as the shutdown can take a very long time if clients are waiting for instances end (or have launched synchronous instances).

## cluster

Use these features if you want to create an high availability cluster with 2 or more nodes.

### cluster.node.name (string) : localhost

Name of the local node. The name MUST be unique amongst the cluster.

### cluster.notify (boolean) : yes

Notify other nodes when configuration changes. Unless you know what you're doing, you should not disable this.

### cluster.notify.user (string) : Ø

User used to connect to other nodes for sending notifications. This user must be ADMIN.

### cluster.notify.password (string) : Ø

Password of the notification user.

### cluster.nodes (string): Ø

A list of coma separated connection strings representing the other nodes of the cluster.

e.g: tcp://192.168.0.1:5000, tcp://192.168.0.2:5000, unix:///run/evqueue/evqueue.socket

It is safe to put here your own node connection string, so this configuration entry should be the same on all the nodes of the cluster.

### cluster.cnx.timeout (numeric) : 10

Connection timeout when connecting to other nodes of the cluster.

### cluster.rcv.timeout (numeric) : 5

TCP receive timeout in seconds.

### cluster.snd.timeout (numeric) : 5

TCP send timeout in seconds.

## datastore

Tasks output are stored in the instance XML. This XML is stored in RAM by the engine as long as the instance is running. If a task outputs lots of data, this can lead to very high memory consumption. This can also be a problem when displaying task output in the web interface.

These parameters are used to control how these data are handled.

### datastore.dom.maxsize (size) : 500K

Maximum output size that will be stored in the XML. If the output exceeds this size, it will be stored in the database. This output will be downloadable in the interface.

### datastore.db.maxsize (size) : 50M

Maximum size that will be stored in database. Output will be truncated if size is above this limit (data will be lost). Use 0 to disable this limit.

### datastore.gzip.enable (boolean) : yes

Whether to enable data compression when downloading database stored outputs.

### datastore.gzip.level (numeric) : 9

Compression level if gzip is enabled.

## dpd

Dead peer detection is used when launching synchronous instances or when waiting for an instance end. This will detect dead clients but also prevent some firewalls to kill connections as they are seen inactive.

### dpd.interval (numeric) : 10

Interval (in seconds) between each DPD packet.

## gc

The garbage collector is used to reduce the size of the evQueue database. It will delete workflow instances after a period of time. Disabling garbage collector will give you an infinite history of terminated workflow instances. This can however cause serious slowdowns on web board (and on search particularly).

### gc.enable (boolean) : yes

Whether or not to enable the garbage collector

### gc.interval (numeric) : 43200

Interval in seconds between executions of the GC

### gc.limit (numeric) : 1000

If entries are to be removed, GC will not free more than this limit at once. You should really use this to avoid long locks on your database.

### gc.delay (numeric) : 2

If the limit is reached, GC will wait this interval (in seconds) before trying to free again up to gc.limit

### gc.logs.retention (numeric) : 7

Clean database logs older than (in days)

### gc.logsapi.retention (numeric) : 30

Clean database API logs older than (in days)

### gc.logsnotifications.retention (numeric) : 30

Clean database notifications error logs older than (in days)

### gc.workflowinstance.retention (numeric) : 30

Clean terminated workflow instances older than (in days)

### gc.uniqueaction.retention (numeric) : 30

When working in a clustered environment, nodes have to elect master node to do some maintenance actions (like GC). These elections are made with the database as a backend and the result is kept this number of days. This is internal to evQueue and you should normally not change this value.

## git

A git repository can be used to save or load workflows and tasks. This is very useful for backup and history purpose, but also if you are running multiple environments (like development and production).

### git.repository (string) : Ø

Path of a local git repository used to update workflows, or to commit local changes.

### git.user (string) : Ø

Git user, if required by the server.

### git.password (string) : Ø

Git password.

### git.public_key (string) : Ø

SSH public key that will be used to connect to the repository, if required.

### git.private_key (string) : Ø

Git private key.

### git.signature.name (string) : 'evQueue'

Signature on commits made from the web interface.

### git.signature.email (string) : evqueue@local

Email of the signature (for commits).

### git.workflows.subdirectory (string) : workflows

The subdirectory of the repository where the workflows should be stored.

## logger

Core engine logs.

### logger.syslog.enable (boolean) : yes

Log errors to syslog. It is recommended to always keep this active.

### logger.syslog.filter : LOG_NOTICE

Only log events with a priority greater or equels to this filter

### logger.db.enable (boolean) : no

Log errors to database. This is required if you want to access logs from the web board. This can slow down the engine on busy systems and thus it is recommended to keep the filter on LOG_WARNING in production.

### logger.db.filter : LOG_WARNING

Only log events with a priority greater or equal to this filter

## loggerapi

Core engine API logs.

### loggerapi.enable (boolean) : yes

Enable API action logs. All API actions that are not readonly will be logged with some details and the user that has made the modification.

## mysql

MySQL configuration

### mysql.database (string) : evqueue

MySQL database name

### mysql.host (string) : localhost

MySQL database host

### mysql.password (string) : Ø

MySQL password

### mysql.user (string) : Ø

MySQL user

## network

TCP configuration (API)

### network.bind.ip (string) : 127.0.0.1

IP which will be used to bind. Use "*" to bind on all interfaces. Use empty string to disable binding.

### network.bind.port (numeric) : 5000

Port to listen on.

### network.bind.path : /tmp/evqueue.socket

Path of the UNIX socket. This socket can be used in combination with the IP socket. Leave blank if not needed. On production environment, you should use **/run/evqueue/evqueue.socket**. See **evqueue-core.evqueue.service** file in debian directory.

### network.listen.backlog (numeric) : 64

Size of the listen backlog: this is the maximum number of pending connections.

### network.rcv.timeout (numeric) : 30

Receive timeout in seconds

### network.snd.timeout (numeric) : 30

Send timeout in seconds

### network.connections.max (numeric) : 128

Maximum number of simultaneous connections.

## notifications

### notifications.tasks.directory (string) : /usr/share/evqueue/plugins/notifications

Where the notification plugins should be found.

### notifications.tasks.timeout (numeric) : 5

Maximum execution time (in seconds) of a notification. Notification task will be killed passed this time.

### notifications.tasks.concurrency (numeric) : 16

Maximum number of notification scripts that can be run simultaneously.

### notifications.logs.directory (string) : /tmp

Where to store notifications tasks output. These files are used to temporarily store stderr unitil task exit. This log is used only if the notiication task exits with an error code. This log is deleted if notifications.logs.delete is set to yes.

### notifications.logs.maxsize (size) : 16K

Maximum size of the log that will be stored into the database. If the output log is larger, it will be truncated to this size.

### notifications.logs.delete (boolean) : yes

Whether to remove temporary log files after they have been stored in database. It is nos recommended to disable this unless for debugging purpose.

## processmanager

### processmanager.logs.directory (string) : /tmp

Where to store tasks outputs. These files are used to temporarily store stdout and stderr until task exit. They might be deleted on exit (this behaviour is controlled by processmanager.logs.delete).

### processmanager.logs.delete (boolean) : yes

Delete logs at the end of task execution. It is strongly recommended to delete logs as they can quickly grow up

### processmanager.tasks.directory (string) : .

Path to prepend when task filename is relative. When a task is created with a relative filename (not beginning with a “/”), this path is prepended before execution. This is useful if you have custom developed tasks, stored on different locations on different environment. This prevents from storing hard coded paths into database. Using this trick you can dump databases from one environment to another, even if paths are different.

### processmanager.monitor.ssh_key (string) : Ø

### processmanager.monitor.ssh_path (string) : /usr/bin/ssh

SSH configuration is used for remote task execution. If no key is specified, default SSH key of the user running evQueue will be used. If you want to use the default key you must comment "processmanager.monitor.ssh_key"

### processmanager.agent.path (string) : /usr/bin/evqueue_agent

When a task is launched on a distant machine (through SSH), it is possible to use the evQueue agent to enable additional functionalities. This is the path of the agent on the distant machine.

### processmanager.logs.tailsize (size) : 20K

The web interface can display live task output. This is the maximum size that will be displayed.

## queuepool

### queuepool.scheduler (string) : fifo

Default queue scheduler (fifo or prio). This can be overloaded when creating a specific queue.

* FIFO scheduler ensures that tasks that are executed in the order they are queued.

* PRIO scheduler will give priority to tasks of the oldest instance. If you are running multiple instances simultaneously, this will help terminating oldest instances, before executing tasks of the newest ones.

## workflowinstance

### workflowinstance.saveparameters (boolean) : yes

Save workflow parameters in database. This is required if you want to use search on parameters values within the web control interface. As on line is inserted for each parameter of each workflow instance, the table t_workflow_instance_parameters can grow quickly. You should be careful when using this option on high loaded platforms. If you disable this, you won't be able to filter workflow instances based on their input parameters. This however only impacts web control interface, and not the core engine.

### workflowinstance.savepoint.level (numeric) : 2

The savepoint level determines how evQueue synchronizes with the database. This affects performance, as well as the web control interface.

* **0** : Never store workflows in database. This has the best performance as no database is used for execution (database is the only used to read configuration). Choosing this option will disable all history in the web board. However, database will be used during engine restart to keep state of running instances. This setting is recommended only if performance is essential.
* **1** : Save workflows on stop or when the engine is restarted. This is slightly less database intensive than mode 2. The only drawback of this setting is that in case of engine crash, no trace of running workflow will be kept, so it will be impossible to relaunch them.
* **2** : Save workflows on start, stop or when the engine is restarted. This is the recommended setting and is a good compromise between performance and history.
* **3** : Save workflows on each state change. A state change occurs each time a task starts or ends. This can be useful if you want to monitor workflows from database instead of accessing the core engine by the TCP socket. However, be aware that this can generate very high load on MySQL and that this will slow down the engine.

Note that the level 0 or 1 will also affect workflowinstance.saveparameters and disable it (whatever its value was).

### workflowinstance.savepoint.retry.enable (boolean) : yes

### workflowinstance.savepoint.retry.times (numeric) : 2

### workflowinstance.savepoint.retry.wait (numeric) : 2

Retry controls what to do on database errors when saving workflows state (savepoint). If retry is deactivated, a database error will prevent the workflow instance from being archived into the database. However, this does not prevent correct execution of the workflows. The will not be shown in the web board as terminated workflows, also they have been well executed. If you enable retry, evQueue will try database requests “times” with a backup time of “wait” seconds. This prevents you from losing data on tamporary database failure (for example restart). 
