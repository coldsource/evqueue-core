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

#include <WorkflowInstance/WorkflowInstance.h>
#include <Exception/Exception.h>
#include <WorkflowInstance/ExceptionWorkflowContext.h>
#include <XML/XMLUtils.h>
#include <Schedule/Retrier.h>
#include <Logger/Logger.h>
#include <API/Statistics.h>

#include <memory>

using namespace std;

void WorkflowInstance::retry_task(DOMElement task)
{
	unsigned int task_instance_id;
	int retry_delay,retry_times, schedule_level;
	char buf[32];
	char *retry_schedule_c;

	// Check if we have a retry_schedule
	string retry_schedule ;
	if(task.hasAttribute("retry_schedule"))
		retry_schedule = task.getAttribute("retry_schedule");

	if(task.hasAttribute("retry_schedule_level"))
		schedule_level = XMLUtils::GetAttributeInt(task,"retry_schedule_level");
	else
		schedule_level = 0;

	if(retry_schedule!="" && schedule_level==0)
	{
		// Retry schedule has not yet begun, so init sequence
		schedule_update(task,retry_schedule,&retry_delay,&retry_times);
	}
	else
	{
		// Check retry parameters (retry_delay and retry_times) allow at least one retry
		if(!task.hasAttribute("retry_delay"))
		{
			error_tasks++; // We won't retry this task, set error
			update_job_statistics("error_tasks",1,task);
			return;
		}

		retry_delay = XMLUtils::GetAttributeInt(task,"retry_delay");

		if(!task.hasAttribute("retry_times"))
		{
			error_tasks++; // We won't retry this task, set error
			update_job_statistics("error_tasks",1,task);
			return;
		}

		retry_times = XMLUtils::GetAttributeInt(task,"retry_times");
	}

	if(retry_schedule!="" && retry_times==0)
	{
		// We have reached end of the schedule level, update
		schedule_update(task,retry_schedule,&retry_delay,&retry_times);
	}

	if(retry_delay==0 || retry_times==0)
	{
		error_tasks++; // No more retry for this task, set error
		update_job_statistics("error_tasks",1,task);
		return;
	}

	// If retry_retval is specified, only retry on specified retval
	if(task.hasAttribute("retry_retval") && task.getAttribute("retry_retval")!=task.getAttribute("retval"))
	{
			error_tasks++; // We won't retry this task, set error
			update_job_statistics("error_tasks",1,task);
			return;
		}

	// Retry task
	time_t t;
	time(&t);
	t += retry_delay;
	strftime(buf,32,"%Y-%m-%d %H:%M:%S",localtime(&t));
	task.setAttribute("retry_at",buf);

	task.setAttribute("retry_times",to_string(retry_times-1));

	Retrier *retrier = Retrier::GetInstance();
	retrier->InsertTask(this,task,time(0)+retry_delay);

	retrying_tasks++;
	update_job_statistics("retrying_tasks",1,task);
}

void WorkflowInstance::schedule_update(DOMElement task,const string &schedule_name,int *retry_delay,int *retry_times)
{
	int schedule_levels;
	
	// Lookup for specified schedule
	unique_ptr<DOMXPathResult> res(xmldoc->evaluate("schedules/schedule[@name='"+schedule_name+"']",xmldoc->getDocumentElement(),DOMXPathResult::FIRST_RESULT_TYPE));
	if(!res->isNode())
	{
		Logger::Log(LOG_WARNING,"[WID %d] Unknown schedule : '%s'",workflow_instance_id,schedule_name.c_str());
		return;
	}
	
	DOMNode schedule = res->getNodeValue();

	unique_ptr<DOMXPathResult> res2(xmldoc->evaluate("count(level)",schedule,DOMXPathResult::FIRST_RESULT_TYPE));
	schedule_levels = res2->getIntegerValue();

	// Get current task schedule state
	int current_schedule_level;
	if(task.hasAttribute("retry_schedule_level"))
		current_schedule_level = XMLUtils::GetAttributeInt(task,"retry_schedule_level");
	else
		current_schedule_level = 0;

	current_schedule_level++;
	task.setAttribute("retry_schedule_level",to_string(current_schedule_level));

	if(current_schedule_level>schedule_levels)
	{
		*retry_delay = 0;
		*retry_times = 0;
		return; // We have reached the last schedule level
	}

	// Get schedule level details
	unique_ptr<DOMXPathResult> res3(xmldoc->evaluate("level[position()="+to_string(current_schedule_level)+"]",schedule,DOMXPathResult::FIRST_RESULT_TYPE));
	DOMElement schedule_level = (DOMElement)res3->getNodeValue();

	string retry_delay_str = schedule_level.getAttribute("retry_delay");
	string retry_times_str = schedule_level.getAttribute("retry_times");

	// Reset task retry informations, according to new shcedule level
	task.setAttribute("retry_delay",retry_delay_str);
	task.setAttribute("retry_times",retry_times_str);

	*retry_delay = std::stoi(retry_delay_str);
	*retry_times = std::stoi(retry_times_str);
}

bool WorkflowInstance::workflow_ended(void)
{
	if(running_tasks==0 && retrying_tasks==0)
	{
		// End workflow (and notify caller) if no tasks are queued or running at this point
		xmldoc->getDocumentElement().setAttribute("status","TERMINATED");
		if(error_tasks>0)
		{
			xmldoc->getDocumentElement().setAttribute("errors",to_string(error_tasks));

			Statistics::GetInstance()->IncWorkflowInstanceErrors();
		}

		return true;
	}

	return false;
}
