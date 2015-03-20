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

#include <WorkflowParameters.h>

#include <string.h>

WorkflowParameters::WorkflowParameters()
{
	first_parameter = 0;
	current_parameter = 0;
}

WorkflowParameters::~WorkflowParameters()
{
	Parameter *cur = first_parameter, *old;
	while(cur)
	{
		delete[] cur->name;
		delete[] cur->value;
		
		old = cur;
		cur = cur->next_parameter;
		delete old;
	}
}

bool WorkflowParameters::Add(const char *name,const char *value)
{
	// Check for duplicate parameter
	Parameter *cur = first_parameter;
	while(cur)
	{
		if(strcmp(cur->name,name)==0)
			return false;
		
		cur = cur->next_parameter;
	}
	
	Parameter *param = new Parameter;
	
	param->name = new char[strlen(name)+1];
	strcpy(param->name,name);
	
	param->value = new char[strlen(value)+1];
	strcpy(param->value,value);
	
	param->next_parameter = first_parameter;
	first_parameter = param;
	
	return true;
}

void WorkflowParameters::SeekStart()
{
	current_parameter = first_parameter;
}

bool WorkflowParameters::Get(const char **name,const char **value)
{
	if(current_parameter==0)
		return false;
	
	*name = current_parameter->name;
	*value = current_parameter->value;
	
	current_parameter = current_parameter->next_parameter;
	return true;
}
