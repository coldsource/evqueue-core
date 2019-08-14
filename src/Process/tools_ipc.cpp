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

#include <tools_ipc.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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