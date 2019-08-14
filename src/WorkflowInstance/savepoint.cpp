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
#include <Logger/Logger.h>
#include <DB/DB.h>

#include <unistd.h>

#include <memory>

using namespace std;

void WorkflowInstance::RecordSavepoint()
{
	unique_lock<recursive_mutex> llock(lock);

	record_savepoint(true);
}

void WorkflowInstance::record_savepoint(bool force)
{
	// Also workflow XML attributes if necessary
	if(xmldoc->getDocumentElement().getAttribute("status")=="TERMINATED")
	{
		if(savepoint_level==0)
			return; // Even in forced mode we won't store terminated workflows on level 0

		xmldoc->getDocumentElement().setAttribute("end_time",format_datetime());
		xmldoc->getDocumentElement().setAttribute("errors",to_string(error_tasks));
	}
	else if(!force && savepoint_level<=2)
		return; // On level 1 and 2 we only record savepoints on terminated workflows

	string savepoint = xmldoc->Serialize(xmldoc->getDocumentElement());

	// Gather workflow values
	string workflow_instance_host, workflow_instance_start, workflow_instance_end, workflow_instance_status;
	if(xmldoc->getDocumentElement().hasAttribute("host"))
		workflow_instance_host = xmldoc->getDocumentElement().getAttribute("host");

	workflow_instance_start = xmldoc->getDocumentElement().getAttribute("start_time");

	if(xmldoc->getDocumentElement().hasAttribute("end_time"))
		workflow_instance_end = xmldoc->getDocumentElement().getAttribute("end_time");

	workflow_instance_status = xmldoc->getDocumentElement().getAttribute("status");

	int tries = 0;
	do
	{
		if(tries>0)
		{
			Logger::Log(LOG_WARNING,"[ WorkflowInstance::record_savepoint() ] Retrying in %d seconds\n",savepoint_retry_wait);
			sleep(savepoint_retry_wait);
		}

		try
		{
			DB db;

			if(savepoint_level>=2)
			{
				// Workflow has already been insterted into database, just update
				if(xmldoc->getDocumentElement().getAttribute("status")!="TERMINATED")
				{
					// Only update savepoint if workflow is still running
					db.QueryPrintfC("UPDATE t_workflow_instance SET workflow_instance_savepoint=%s WHERE workflow_instance_id=%i",savepoint.c_str(),&workflow_instance_id);
				}
				else
				{
					// Update savepoint and status if workflow is terminated
					db.QueryPrintfC("UPDATE t_workflow_instance SET workflow_instance_savepoint=%s,workflow_instance_status='TERMINATED',workflow_instance_errors=%i,workflow_instance_end=NOW() WHERE workflow_instance_id=%i",savepoint.c_str(),&error_tasks,&workflow_instance_id);
				}
			}
			else
			{
				// Always insert full informations as we are called at workflow end or when engine restarts
				db.QueryPrintfC("\
					INSERT INTO t_workflow_instance(workflow_instance_id,workflow_id,workflow_schedule_id,workflow_instance_host,workflow_instance_start,workflow_instance_end,workflow_instance_status,workflow_instance_errors,workflow_instance_savepoint)\
					VALUES(%i,%i,%i,%s,%s,%s,%s,%i,%s)",
					&workflow_instance_id,&workflow_id,&workflow_schedule_id,workflow_instance_host.length()?workflow_instance_host.c_str():0,workflow_instance_start.c_str(),workflow_instance_end.length()?workflow_instance_end.c_str():"0000-00-00 00:00:00",workflow_instance_status.c_str(),&error_tasks,savepoint.c_str());
			}

			break;
		}
		catch(Exception &e)
		{
			Logger::Log(LOG_ERR,"[ WorkflowInstance::record_savepoint() ] Unexpected exception : [ %s ] %s\n",e.context.c_str(),e.error.c_str());
		}

		tries++;

	} while(savepoint_retry && (savepoint_retry_times==0 || tries<=savepoint_retry_times));
}
