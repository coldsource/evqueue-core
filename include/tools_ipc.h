#ifndef _TOOLS_IPC_H_
#define _TOOLS_IPC_H_

#include <sys/types.h>

key_t ipc_get_qid(const char *qid_istr);
int ipc_openq(const char *qid_str);

#endif