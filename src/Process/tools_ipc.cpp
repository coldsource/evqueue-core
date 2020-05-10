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

#include <Process/tools_ipc.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <global.h>

key_t ipc_get_qid(const char *qid_str)
{
	if(qid_str[0]=='/')
		return ftok(qid_str,0xE7);
	if(strncmp(qid_str,"0x",2)==0)
		return strtol(qid_str,0,16);
	return strtol(qid_str,0,10);
}

int ipc_openq(const char *qid_str)
{
	int msgqid = msgget(ipc_get_qid(qid_str), 0700 | IPC_CREAT);
	
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

int ipc_queue_destroy(const char *qid_str)
{
	int msgqid = ipc_openq(qid_str);
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

int ipc_queue_stats(const char *qid_str)
{
	int msgqid = ipc_openq(qid_str);
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
	
	return 0;
}

int ipc_send_exit_msg(const char *qid_str,int type,int tid,char retcode)
{
	int msgqid = ipc_openq(qid_str);
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

bool ipc_send_progress_message(int msgqid,const char* buf,pid_t tid)
{
	if(buf[0]!='%' || buf[1]=='%')
		return  false;
	
	int prct = atoi(buf+1);
	if(prct<0)
		prct = 0;
	else if(prct>100)
		prct = 100;
	
	st_msgbuf msgbuf_progress;
	msgbuf_progress.type = 3;
	msgbuf_progress.mtext.pid = getpid();
	msgbuf_progress.mtext.tid = tid;
	msgbuf_progress.mtext.retcode = prct;
	
	msgsnd(msgqid,&msgbuf_progress,sizeof(st_msgbuf::mtext),0);
	
	return  true;
}
