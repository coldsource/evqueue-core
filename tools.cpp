#include <tools.h>
#include <global.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <stdio.h>

int tools_destroy_queue()
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