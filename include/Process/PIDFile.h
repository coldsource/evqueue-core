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

#ifndef _PIDFILE_H_
#define _PIDFILE_H_

#include <sys/types.h>

#include <string>

class PIDFile
{
	std::string filename;
	FILE *pidfile;
	
	public:
		PIDFile(const std::string &name, const std::string &filename, pid_t pid = 0);
		void Write(pid_t pid);
		~PIDFile();
};

#endif
