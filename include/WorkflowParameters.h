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

#ifndef _WORKFLOWPARAMETERS_H_
#define _WORKFLOWPARAMETERS_H_

#include <string>
#include <vector>

class WorkflowParameters
{
	private:
		struct Parameter
		{
			std::string name;
			std::string value;
		};
		
		std::vector<Parameter> parameters;
		int current_parameter; // For iteration
	
	public:
		WorkflowParameters();
		~WorkflowParameters();
		
		bool Add(const char *name,const char *value);
		void SeekStart();
		bool Get(const char **name,const char **value);
};

#endif
