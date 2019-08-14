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

#include <Workflow/WorkflowParameters.h>

#include <string.h>

using namespace std;

WorkflowParameters::WorkflowParameters()
{
	current_parameter = 0;
}

WorkflowParameters::~WorkflowParameters()
{

}

bool WorkflowParameters::Add(const string &name,const string &value)
{
	// Check for duplicate parameter
	for(int i=0;i<parameters.size();i++)
	{
		if(parameters.at(i).name==name)
			return false;
	}
	
	Parameter param;
	param.name = name;
	param.value = value;
	
	parameters.push_back(param);
	
	return true;
}

void WorkflowParameters::SeekStart()
{
	current_parameter = 0;
}

bool WorkflowParameters::Get(string &name,string &value)
{
	if(current_parameter>=parameters.size())
		return false;
	
	name = parameters.at(current_parameter).name;
	value = parameters.at(current_parameter).value;
	
	current_parameter++;;
	return true;
}

bool WorkflowParameters::Get(string **name,string **value)
{
	if(current_parameter>=parameters.size())
		return false;
	
	*name = &(parameters.at(current_parameter).name);
	*value = &(parameters.at(current_parameter).value);
	
	current_parameter++;;
	return true;
}