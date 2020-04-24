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

#include <API/ActiveConnections.h>
#include <Logger/Logger.h>
#include <API/handle_connection.h>
#include <WS/WSServer.h>

using namespace std;

ActiveConnections *ActiveConnections::instance = 0;

ActiveConnections::ActiveConnections()
{
	instance = this;
	is_shutting_down = false;
}

void ActiveConnections::StartAPIConnection(int s)
{
	unique_lock<mutex> llock(lock);
	
	if(is_shutting_down)
		return;
	
	Logger::Log(LOG_DEBUG, "Accepting new connection, current connections : %d",active_api_threads.size()+1);
	
	thread t(handle_connection, s);
	active_api_threads[t.get_id()] = move(t);
}

void ActiveConnections::EndAPIConnection(thread::id thread_id)
{
	unique_lock<mutex> llock(lock);
	
	if(is_shutting_down)
		return;
	
	active_api_threads.at(thread_id).detach();
	active_api_threads.erase(thread_id);
	
	Logger::Log(LOG_DEBUG, "Ending connection, current connections : %d",active_api_threads.size());
}

void ActiveConnections::StartWSConnection(int s)
{
	unique_lock<mutex> llock(lock);
	
	if(is_shutting_down)
		return;
	
	Logger::Log(LOG_DEBUG, "Accepting new WS connection, current connections : %d",active_ws_sockets.size()+1);
	
	active_ws_sockets.insert(s);
	WSServer::GetInstance()->Adopt(s);
}

void ActiveConnections::EndWSConnection(int s)
{
	unique_lock<mutex> llock(lock);
	
	if(is_shutting_down)
		return;
	
	active_ws_sockets.erase(s);
	
	Logger::Log(LOG_DEBUG, "Ending WS connection, current connections : %d",active_ws_sockets.size());
}

unsigned int ActiveConnections::GetAPINumber()
{
	unique_lock<mutex> llock(lock);
	
	int n = active_api_threads.size();
	
	return n;
}

unsigned int ActiveConnections::GetWSNumber()
{
	unique_lock<mutex> llock(lock);
	
	int n = active_ws_sockets.size();
	
	return n;
}

void ActiveConnections::Shutdown(void)
{
	unique_lock<mutex> llock(lock);
	
	is_shutting_down = true;
}

void ActiveConnections::WaitForShutdown()
{
	// Join all threads to ensure no more connections are active
	for(auto it=active_api_threads.begin();it!=active_api_threads.end();it++)
		it->second.join();
}
