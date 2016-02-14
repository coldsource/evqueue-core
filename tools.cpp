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

#include <tools.h>
#include <tools_ipc.h>
#include <global.h>
#include <Logger.h>
#include <WorkflowScheduler.h>
#include <Tasks.h>
#include <RetrySchedules.h>
#include <Workflows.h>
#include <Notifications.h>
#include <Retrier.h>
#include <Configuration.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

using namespace std;

int tools_queue_destroy()
{
	int msgqid = ipc_openq(Configuration::GetInstance()->Get("core.ipc.qid").c_str());
	if(msgqid==-1)
		return -1;
	
	int re = msgctl(msgqid,IPC_RMID,0);
	if(re!=0)
	{
		if(errno==EPERM)
			fprintf(stderr,"Permission refused while trying to remove message queue\n");
		else
			fprintf(stderr,"Unknown error trying to remove message queue : %d\n",errno);
		
		return -1;
	}
	
	printf("Message queue successfully removed\n");
	
	return 0;
}

int tools_queue_stats()
{
int msgqid = ipc_openq(Configuration::GetInstance()->Get("core.ipc.qid").c_str());
	if(msgqid==-1)
		return -1;
	
	struct msqid_ds ipcq_stats;
	int re = msgctl(msgqid,IPC_STAT,&ipcq_stats);
	if(re!=0)
	{
		fprintf(stderr,"Unknown error trying to get message queue statistics: %d\n",errno);
		return -1;
	}
	
	printf("Queue size : %ld\n",ipcq_stats.msg_qbytes);
	printf("Pending messages : %ld\n",ipcq_stats.msg_qnum);
}

void tools_print_usage()
{
	fprintf(stderr,"Usage :\n");
	fprintf(stderr,"  Launch evqueue      : evqueue (--daemon) --config <path to config file>\n");
	fprintf(stderr,"  Show version        : evqueue --version\n");
	fprintf(stderr,"  Clean IPC queue     : evqueue --ipcq-remove\n");
	fprintf(stderr,"  Get IPC queue stats : evqueue --ipcq-stats\n");
}

void tools_config_reload(void)
{
	Logger::Log(LOG_INFO,"Got SIGHUP, reloading scheduler configuration");
	
	WorkflowScheduler *scheduler = WorkflowScheduler::GetInstance();
	scheduler->Reload();
	
	Tasks *tasks = Tasks::GetInstance();
	tasks->Reload();
	
	RetrySchedules *retry_schedules = RetrySchedules::GetInstance();
	retry_schedules->Reload();
	
	Workflows *workflows = Workflows::GetInstance();
	workflows->Reload();
	
	Notifications *notifications = Notifications::GetInstance();
	notifications->Reload();
}

void tools_sync_tasks(void)
{
	Tasks *tasks = Tasks::GetInstance();
	tasks->SyncBinaries();
}

void tools_flush_retrier(void)
{
	Logger::Log(LOG_INFO,"Flushing retrier");
	Retrier *retrier = Retrier::GetInstance();
	retrier->Flush();
}

int tools_send_exit_msg(int type,int tid,char retcode)
{
	int msgqid = ipc_openq(Configuration::GetInstance()->Get("core.ipc.qid").c_str());
	if(msgqid==-1)
		return -1;
	
	st_msgbuf msgbuf;
	msgbuf.type = type;
	memset(&msgbuf.mtext,0,sizeof(st_msgbuf::mtext));
	msgbuf.mtext.pid = getpid();
	msgbuf.mtext.tid = tid;
	msgbuf.mtext.retcode = retcode;
	return msgsnd(msgqid,&msgbuf,sizeof(st_msgbuf::mtext),0);
}

