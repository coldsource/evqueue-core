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

class Cluster
{
	std::vector<std::string> nodes;
	
	public:
		void ParseConfiguration(const std::string &conf);
		
		void ExecuteCommand(const std::string &command);
	
	private:
		void execute_command(const std::string &cnx_str, const std::string &command);
		int connect_socket(const std::string &connection_str_const);
};

#endif