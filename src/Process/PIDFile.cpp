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

#include <Process/PIDFile.h>
#include <Exception/Exception.h>

#include <stdio.h>
#include <unistd.h>

using namespace std;

PIDFile::PIDFile(const string &name, const string &filename, pid_t pid)
{
	this->filename = filename;
	
	pidfile = fopen(filename.c_str(),"w");
	if(pidfile==0)
		throw Exception("core","Unable to open "+name+" pid file");
	
	if(pid)
		Write(pid);
}

PIDFile::~PIDFile()
{
	unlink(filename.c_str());
}

void PIDFile::Write(pid_t pid)
{
	fprintf(pidfile,"%d\n",pid);
	fclose(pidfile);
}
