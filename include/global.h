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

#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#define WORKFLOW_NAME_MAX_LEN           64
#define QUEUE_NAME_MAX_LEN              64
#define RETRY_SCHEDULE_NAME_MAX_LEN     64
#define TASK_NAME_MAX_LEN               64
#define TASK_BINARY_MAX_LEN            128
#define PARAMETERS_MAX_LEN     (1024*1024)
#define PARAMETER_NAME_MAX_LEN          64
#define USER_NAME_MAX_LEN               32
#define CHANNEL_NAME_MAX_LEN            32
#define CHANNEL_GROUP_NAME_MAX_LEN      32
#define CHANNEL_FIELD_NAME_MAX_LEN      32
#define ALERT_NAME_MAX_LEN              64

#include <unistd.h>

struct st_msgbuf
{
	long type;
	
	struct {
		pid_t pid;
		pid_t tid;
		char retcode;
	} mtext;
};

// Global MACROS
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

extern int g_argc;
extern char **g_argv;

// Logs fileno
#define LOG_FILENO 3

#endif
