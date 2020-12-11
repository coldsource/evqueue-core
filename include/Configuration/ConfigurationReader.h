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

#ifndef _CONFIGURATIONREADER_H_
#define _CONFIGURATIONREADER_H_

#define CONFIGURATION_LINE_MAXLEN           4096

#include <Configuration/Configuration.h>

#include <string>
#include <vector>

class ConfigurationReader
{
	public:
		static void Read(const std::string &filename, Configuration *config);
		static void ReadDefaultPaths(const std::string &filename, Configuration *config);
		
		static int ReadCommandLine(int argc, char **argv, const std::vector<std::string> &filter, const std::string &prefix, Configuration *config);
};

#endif
