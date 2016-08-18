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

#include <Sockets.h>

#include <unistd.h>

using namespace std;

Sockets *Sockets::instance = 0;

Sockets::Sockets()
{
	pthread_mutex_init(&lock, NULL);
	
	instance = this;
}

void Sockets::RegisterSocket(int s)
{
	pthread_mutex_lock(&lock);
	
	sockets.insert(s);
	
	pthread_mutex_unlock(&lock);
}

void Sockets::UnregisterSocket(int s)
{
	pthread_mutex_lock(&lock);
	
	sockets.erase(s);
	close(s);
	
	pthread_mutex_unlock(&lock);
}

void Sockets::CloseSockets()
{
	pthread_mutex_lock(&lock);
	
	for(set<int>::iterator it=sockets.begin();it!=sockets.end();it++)
		close(*it);
	
	pthread_mutex_unlock(&lock);
}

unsigned int Sockets::GetNumber()
{
	pthread_mutex_lock(&lock);
	
	int n = sockets.size();
	
	pthread_mutex_unlock(&lock);
	
	return n;
}

void Sockets::Lock()
{
	pthread_mutex_lock(&lock);
}

void Sockets::Unlock()
{
	pthread_mutex_unlock(&lock);
}