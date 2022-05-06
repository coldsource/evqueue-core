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

#ifndef _CONFIGURATION_H_
#define _CONFIGURATION_H_

#include <string>
#include <map>

class Configuration
{
	protected:
		std::map<std::string,std::string> entries;
	
	public:
		Configuration(void);
		Configuration(const std::map<std::string,std::string> &entries);
		~Configuration(void);
		
		bool Set(const std::string &entry,const std::string &value);
		const std::string &Get(const std::string &entry) const;
		int GetInt(const std::string &entry) const;
		int GetSize(const std::string &entry) const;
		bool GetBool(const std::string &entry) const;
		int GetUID(const std::string &entry) const;
		int GetGID(const std::string &entry) const;

	protected:
		void check_f_is_exec(const std::string &filename);
		void check_d_is_writeable(const std::string &path);
		void check_bool_entry(const std::string &name);
		void check_int_entry(const std::string &name, bool signed_int=false);
		void check_size_entry(const std::string &name);
};

#endif
