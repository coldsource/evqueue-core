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

#ifndef _APIOBJECTLIST_H_
#define _APIOBJECTLIST_H_

#include <Exception/Exception.h>

#include <string>
#include <map>
#include <mutex>

template<typename APIObjectType>
class APIObjectList
{
	protected:
		std::mutex lock;
		
		std::map<unsigned int,APIObjectType *> objects_id;
		std::map<std::string,APIObjectType *> objects_name;
		
		void add(unsigned int id, const std::string &name, APIObjectType *object)
		{
			if(id)
				objects_id[id] = object;
			
			if(name.length())
				objects_name[name] = object;
		}
		
		void clear()
		{
			// Clean objects
			if(objects_id.size())
			{
				for(auto it=objects_id.begin();it!=objects_id.end();++it)
					delete it->second;
			}
			else if(objects_name.size())
			{
				for(auto it=objects_name.begin();it!=objects_name.end();++it)
					delete it->second;
			}
			
			// Empy maps
			objects_id.clear();
			objects_name.clear();
		}
	
	public:
		
		APIObjectList()
		{
			;
		}
		
		~APIObjectList()
		{
			clear();
		}
		
		bool Exists(unsigned int id)
		{
			std::unique_lock<std::mutex> llock(lock);
			
			auto it = objects_id.find(id);
			if(it==objects_id.end())
				return false;
			
			return true;
		}
		
		bool Exists(const std::string &name)
		{
			std::unique_lock<std::mutex> llock(lock);
			
			auto it = objects_name.find(name);
			if(it==objects_name.end())
				return false;
			
			return true;
		}
		
		const APIObjectType Get(unsigned int id)
		{
			std::unique_lock<std::mutex> llock(lock);
			
			auto it = objects_id.find(id);
			if(it==objects_id.end())
				throw Exception("API","Unknown object ID : " + std::to_string(id),"UNKNOWN_OBJECT");
			
			APIObjectType object = *it->second;
			
			return object;
		}
		
		const APIObjectType Get(const std::string &name)
		{
			std::unique_lock<std::mutex> llock(lock);
			
			auto it = objects_name.find(name);
			if(it==objects_name.end())
				throw Exception("API","Unknown object name : " + name,"UKNOWN_OBJECT");
			
			APIObjectType object = *it->second;
			
			return object;
		}
};

#endif
