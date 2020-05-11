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

#ifndef _FORKER_H_
#define _FORKER_H_

#include <unistd.h>

#include <string>
#include <mutex>

class Task;

class Forker
{
	static Forker *instance;
	
	int pipe_evq_to_forker[2];
	int pipe_forker_to_evq[2];
	
	unsigned int pipe_id = 0;
	std::string pipes_directory;
	std::string node_name;
	
	std::mutex lock;
	
	static void signal_callback_handler(int signum);
	
public:
	Forker();
	
	static Forker *GetInstance() { return instance; }
	
	pid_t Start();
	void Init();
	
	pid_t Execute(const std::string &type, const std::string &data);
};

#endif
