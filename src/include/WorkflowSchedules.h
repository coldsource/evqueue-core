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

#include <map>
#include <vector>

#include <APIObjectList.h>

class WorkflowSchedule;
class SocketQuerySAX2Handler;
class QueryResponse;

class WorkflowSchedules:public APIObjectList<WorkflowSchedule>
{
	static WorkflowSchedules *instance;
		
	std::vector<WorkflowSchedule *> active_schedules;
	
	public:
		WorkflowSchedules();
		~WorkflowSchedules();
		
		static WorkflowSchedules *GetInstance() { return instance; }
		
		void Reload(void);
		
		const std::vector<WorkflowSchedule *> &GetActiveWorkflowSchedules();
		
		static bool HandleQuery(SocketQuerySAX2Handler *saxh, QueryResponse *response);
};