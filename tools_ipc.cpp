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
