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

#include <Process/tools_proc.h>
#include <global.h>

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

// Clean all unneeded open file descriptors
void sanitize_fds(int start)
{
	int od_fd = open("/proc/self/fd", O_DIRECTORY);
	if(od_fd<0)
	{
		fprintf(stderr, "Error opening /proc/self/fd\n");
		return;
	}
	
	DIR *dh = fdopendir(od_fd);
	if(dh==0)
	{
		fprintf(stderr, "Error opening /proc/self/fd\n");
		return;
	}
	
	struct dirent *result;
	while(true)
	{
		errno = 0;
		result = readdir(dh);
		if(result==0 && errno!=0)
		{
			fprintf(stderr, "Got error %d while reading /proc/self/fd\n",errno);
			break;
		}

		if(result==0)
			break;

		if(result->d_name[0]=='.')
			continue; // Skip hidden files

		int fd = atoi(result->d_name);
		if(fd>=start && fd!=od_fd && fd<1024) // Do not clean FD above 1024 to avoir breaking valgrind
			close(fd);
	}
	
	closedir(dh);
	close(od_fd);
}

void setproctitle(const char *title)
{
	char *start = g_argv[0];
	char *end = g_argv[g_argc-1]+strlen(g_argv[g_argc-1]);
	memset(start,0,end-start);
	memcpy(start,title,MIN(strlen(title), end-start-1));
}
