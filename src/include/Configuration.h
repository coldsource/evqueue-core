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

class QueryResponse;

class Configuration
{
	std::map<std::string,std::string> entries;
	
	static Configuration *instance;
	
	public:
		Configuration(void);
		~Configuration(void);
		
		bool Set(const std::string &entry,const std::string &value);
		const std::string &Get(const std::string &entry) const;
		int GetInt(const std::string &entry) const;
		int GetSize(const std::string &entry) const;
		bool GetBool(const std::string &entry) const;
		
		void Substitute(void);

		void Check(void);
		
		void SendConfiguration(QueryResponse *response);
		
		static inline Configuration *GetInstance(void) { return instance; }

	private:
		void check_f_is_exec(const std::string &filename);
		void check_d_is_writeable(const std::string &path);
		void check_bool_entry(const std::string &name);
};

#endif
