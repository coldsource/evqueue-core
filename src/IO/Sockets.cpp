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

#include <IO/Sockets.h>

#include <unistd.h>
#include <sys/socket.h>

using namespace std;

Sockets *Sockets::instance = 0;

Sockets::Sockets()
{
	instance = this;
}

void Sockets::RegisterSocket(int s)
{
	unique_lock<mutex> llock(lock);
	
	sockets.insert(s);
}

void Sockets::UnregisterSocket(int s)
{
	unique_lock<mutex> llock(lock);
	
	sockets.erase(s);
	close(s);
}

void Sockets::CloseSockets()
{
	unique_lock<mutex> llock(lock);
	
	for(set<int>::iterator it=sockets.begin();it!=sockets.end();it++)
		close(*it);
}

void Sockets::ShutdownSockets()
{
	unique_lock<mutex> llock(lock);
	
	for(set<int>::iterator it=sockets.begin();it!=sockets.end();it++)
		shutdown(*it,SHUT_RDWR);
}

unsigned int Sockets::GetNumber()
{
	unique_lock<mutex> llock(lock);
	
	return sockets.size();
}

void Sockets::Lock()
{
	lock.lock();
}

void Sockets::Unlock()
{
	lock.unlock();
}