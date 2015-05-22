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
#include <global.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <stdio.h>

static int openipcq()
{
	int msgqid = msgget(PROCESS_MANAGER_MSGQID,0);
	if(msgqid==-1)
	{
		if(errno==EACCES)
			fprintf(stderr,"Permission refused while trying to open message queue\n");
		else if(errno==ENOENT)
			fprintf(stderr,"No message queue found\n");
		else
			fprintf(stderr,"Unknown error trying to open message queue : %d\n",errno);
		
		return -1;
	}
	
	return msgqid;
}

int tools_queue_destroy()
{
	int msgqid = openipcq();
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
int msgqid = openipcq();
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
	fprintf(stderr,"  Clean IPC queue     : evqueue --ipcq-remove\n");
	fprintf(stderr,"  Get IPC queue stats : evqueue --ipcq-stats\n");
}

int ipc_send_exit_msg(int type,int tid,char retcode)
{
	int msgqid = msgget(PROCESS_MANAGER_MSGQID,0700 | IPC_CREAT);
	if(msgqid==-1)
		return -1;
	
	st_msgbuf msgbuf;
	msgbuf.type = type;
	msgbuf.mtext.pid = getpid();
	msgbuf.mtext.tid = tid;
	msgbuf.mtext.retcode = retcode;
	return msgsnd(msgqid,&msgbuf,sizeof(st_msgbuf::mtext),0);
}