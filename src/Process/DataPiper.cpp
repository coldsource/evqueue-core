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

#include <Process/DataPiper.h>
#include <Exception/Exception.h>

#include <signal.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>

using namespace std;

DataPiper *DataPiper::instance = 0;

DataPiper::DataPiper()
{
	if(pipe(self_pipe)!=0)
		throw Exception("DataPiper","Unable to create pipe, could not start");
	
	instance = this;
	
	dp_thread_handle = thread(DataPiper::dp_thread,this);
}

void DataPiper::PipeData(int fd, const string &data)
{
	unique_lock<mutex> llock(lock);
	 
	// Set nonblocking IO mode
	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	 
	// Wake up poll() to add this FD. Since IO is non-blocking we have to use a static buffer
	static char buf[1] = {'\0'};
	size_t written = write(self_pipe[1],buf,1);
	if(written!=1)
	{
		close(fd);
		throw Exception("DataPiper","Error writing to self pipe, unable to pipe data to child");
	}
	
	// Store FD and data to be sent
	pipe_data[fd] = {data, 0};
}

void DataPiper::dp_thread(DataPiper *dp)
{
	syslog(LOG_NOTICE,"Data piper started");
	
	while(true)
	{
		// Prepare list of FD on which we want to write data
		dp->lock.lock();
		
		int nfds = dp->pipe_data.size();
		
		struct pollfd fds[nfds+1];
		int i = 0;
		for(auto it=dp->pipe_data.begin();it!=dp->pipe_data.end();++it, ++i)
		{
			fds[i].fd = it->first;
			fds[i].events = POLLOUT|POLLRDHUP;
		}
		
		// Add self pipe
		fds[nfds].fd = dp->self_pipe[0];
		fds[nfds].events = POLLIN;
		
		dp->lock.unlock();
		
		poll(fds,nfds+1,-1);
		
		char buf[1];
		if(fds[nfds].revents&POLLIN)
		{
			// We were woken by self pipe, read the byte to avoid looping
			size_t read_size = read(fds[nfds].fd,buf,1);
			if(read_size!=1)
				syslog(LOG_WARNING,"DataPiper : unable to read self pipe");
			
			if(dp->pipe_data.size()==0 && dp->is_shutting_down)
			{
				// Shutdown is requested and we are done piping data, we can exit now
				syslog(LOG_NOTICE,"Shutdown requested, exiting data piper");
				return;
			}
		}
		
		dp->lock.lock();
		
		for(int i=0;i<nfds;i++)
		{
			if(fds[i].revents&POLLOUT)
			{
				// We can write on this FD
				int to_write = dp->pipe_data[fds[i].fd].data.length()-dp->pipe_data[fds[i].fd].written;
				if(to_write>4096)
					to_write = 4096; // Write no more than one page
				
				int re;
				// This should be non-blocking
				re = write(fds[i].fd,dp->pipe_data[fds[i].fd].data.c_str()+dp->pipe_data[fds[i].fd].written,to_write);
				if(re<0)
				{
					if(errno==EAGAIN) // Write blocked (too big ?), at least write one byte
						re = write(fds[i].fd,dp->pipe_data[fds[i].fd].data.c_str()+dp->pipe_data[fds[i].fd].written,1);
				}
				
				if(re>0)
					dp->pipe_data[fds[i].fd].written += re;
				
				// Error occured or write is finished, close FD and remove it
				if(re<=0 || dp->pipe_data[fds[i].fd].data.length()==dp->pipe_data[fds[i].fd].written)
				{
					close(fds[i].fd);
					dp->pipe_data.erase(fds[i].fd);
					
					if(dp->pipe_data.size()==0 && dp->is_shutting_down)
					{
						// Shutdown is requested and we are done piping data, we can exit now
						syslog(LOG_NOTICE,"Shutdown requested, exiting data piper");
						return;
					}
				}
			}
		}
		
		dp->lock.unlock();
	}
}

void DataPiper::Shutdown()
{
	unique_lock<mutex> llock(lock);
	
	is_shutting_down = true;
	
	// Wake poll()
	static char buf[1] = {'\0'};
	if(write(self_pipe[1],buf,1)!=1)
		syslog(LOG_WARNING,"DataPiper: could not write to selfpipe, shutdown cancelled");
}
	
void DataPiper::WaitForShutdown()
{
	dp_thread_handle.join();
}
