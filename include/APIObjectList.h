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

#include <Exception.h>

#include <pthread.h>

#include <string>
#include <map>

template<typename APIObjectType>
class APIObjectList
{
	protected:
		pthread_mutex_t lock;
		
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
			pthread_mutex_init(&lock, NULL);
		}
		
		~APIObjectList()
		{
			clear();
		}
		
		bool Exists(unsigned int id)
		{
			pthread_mutex_lock(&lock);
			
			auto it = objects_id.find(id);
			if(it==objects_id.end())
			{
				pthread_mutex_unlock(&lock);
				
				return false;
			}
			
			pthread_mutex_unlock(&lock);
			
			return true;
		}
		
		bool Exists(const std::string &name)
		{
			pthread_mutex_lock(&lock);
			
			auto it = objects_name.find(name);
			if(it==objects_name.end())
			{
				pthread_mutex_unlock(&lock);
				
				return false;
			}
			
			pthread_mutex_unlock(&lock);
			
			return true;
		}
		
		APIObjectType Get(unsigned int id)
		{
			pthread_mutex_lock(&lock);
			
			auto it = objects_id.find(id);
			if(it==objects_id.end())
			{
				pthread_mutex_unlock(&lock);
				
				throw Exception("API","Unknown object ID");
			}
			
			APIObjectType object = *it->second;
			
			pthread_mutex_unlock(&lock);
			
			return object;
		}
		
		APIObjectType Get(const std::string &name)
		{
			pthread_mutex_lock(&lock);
			
			auto it = objects_name.find(name);
			if(it==objects_name.end())
			{
				pthread_mutex_unlock(&lock);
				
				throw Exception("API","Unknown object name");
			}
			
			APIObjectType object = *it->second;
			
			pthread_mutex_unlock(&lock);
			
			return object;
		}
};

#endif