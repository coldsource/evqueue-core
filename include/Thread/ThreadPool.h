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

#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

#include <vector>

template<typename ThreadType>
class ThreadPool
{
	std::vector<ThreadType *> threads;
	
	public:
		template<typename ...Args>
		ThreadPool(int n, Args... args)
		{
			for(int i=0;i<n;i++)
				threads.push_back(new ThreadType(args...));
		}
		
		void Shutdown()
		{
			for(int i=0;i<threads.size();i++)
			{
				threads[i]->Shutdown();
				delete threads[i];
			}
		}
};

#endif
