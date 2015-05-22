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

#ifndef _WORKFLOW_H_
#define _WORKFLOW_H_

#include <vector>
#include <string>

class DB;

class Workflow
{
	unsigned int workflow_id;
	std::string workflow_name;
	std::string workflow_xml;
	
	std::vector<unsigned int> notifications;
		
	public:
		Workflow();
		Workflow(DB *db,const char *workflow_name);
		
		unsigned int GetID() const { return workflow_id; }
		const char *GetName() const { return workflow_name.c_str(); }
		const char *GetXML() const { return workflow_xml.c_str(); }
		std::vector<unsigned int> GetNotifications() const { return notifications; }
};

#endif
