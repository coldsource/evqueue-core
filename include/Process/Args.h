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

#ifndef _ARGS_H_
#define _ARGS_H_

#include <string>
#include <map>

class Args
{
	 std::map<std::string, std::string> vals;
	 std::map<std::string, std::string> types;
	 std::map<std::string, bool> required;
	 
	 class args_val
	 {
		std::string name;
		 std::string val;
		std::string type;
		
		public:
			args_val(const std::string &name, const std::string val, const std::string type);
			
			operator bool() const;
			operator int() const;
			operator std::string() const;
	 };
	
	public:
		Args() {}
		Args(const std::map<std::string, std::string> &config,int argc, char **argv);
		
		args_val operator[](const std::string &name);
};

#endif
