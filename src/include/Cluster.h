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

#ifndef _CLUSTER_H_
#define _CLUSTER_H_

#include <vector>
#include <string>

class DOMDocument;

class Cluster
{
	static Cluster *instance;
	
	bool  notify;
	
	std::vector<std::string> nodes;
	std::string user;
	std::string password;
	
	int cnx_timeout;
	int snd_timeout;
	int rcv_timeout;
	
	public:
		void ParseConfiguration(const std::string &conf);
		
		static Cluster *GetInstance() { return instance; }
		
		void Notify(const std::string &command);
		bool ExecuteCommand(const std::string &command, DOMDocument *response = 0);
};

#endif