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
#include <Queue/QueuePool.h>
#include <XML/XMLUtils.h>
#include <Configuration/ConfigurationEvQueue.h>
#include <DB/DB.h>
#include <WS/Events.h>

#include <memory>

using namespace std;

void WorkflowInstance::Start(bool *workflow_terminated)
{
	unique_lock<recursive_mutex> llock(lock);

	Logger::Log(LOG_INFO,"[WID %d] Started",workflow_instance_id);

	xmldoc->getDocumentElement().setAttribute("status","EXECUTING");
	xmldoc->getDocumentElement().setAttribute("start_time",format_datetime());

	try
	{
		run_subjobs(xmldoc->getDocumentElement());
	}
	catch(Exception &e)
	{
		// Nothing to do on error, just let the workflow end itself since tasks might still be running
	}

	*workflow_terminated = workflow_ended();

	record_savepoint();
	
	Events::GetInstance()->Create("INSTANCE_STARTED",workflow_instance_id);
}

void WorkflowInstance::Resume(bool *workflow_terminated)
{
	QueuePool *qp = QueuePool::GetInstance();

	unique_lock<recursive_mutex> llock(lock);
	
	// Clear statistics
	clear_statistics();
	
	// Look for tasks or job that have conditions waiting for a new evaluation (via evqWait() function)
	{
		unique_ptr<DOMXPathResult> res(xmldoc->evaluate("//*[(name() = 'job' or name() = 'task') and @status='WAITING']",xmldoc->getDocumentElement(),DOMXPathResult::SNAPSHOT_RESULT_TYPE));
		int i = 0;
		while(res->snapshotItem(i++))
		{
			waiting_nodes.push_back((DOMElement)res->getNodeValue());
			waiting_conditions++;
			update_job_statistics("waiting_conditions",1,(DOMElement)res->getNodeValue());
		}
	}

	// Look for EXECUTING tasks (they might be still in execution or terminated)
	// Also look for TERMINATED tasks because we might have to retry them
	unique_ptr<DOMXPathResult> tasks(xmldoc->evaluate("//task[@status='QUEUED' or @status='EXECUTING' or @status='TERMINATED']",xmldoc->getDocumentElement(),DOMXPathResult::SNAPSHOT_RESULT_TYPE));
	DOMElement task;

	int tasks_index = 0;
	while(tasks->snapshotItem(tasks_index++))
	{
		task = (DOMElement)tasks->getNodeValue();

		if(task.getAttribute("status")=="QUEUED")
			enqueue_task(task);
		else if(task.getAttribute("status")=="EXECUTING")
		{
			// Restore mapping tables in QueuePool for executing tasks
			string queue_name = task.getAttribute("queue");
			pid_t task_id = XMLUtils::GetAttributeInt(task,"tid");
			qp->ExecuteTask(this,task,queue_name,task_id);

			running_tasks++;
			update_job_statistics("running_tasks",1,task);
		}
		else if(task.getAttribute("status")=="TERMINATED")
		{
			if(task.getAttribute("retval")!="0")
				retry_task(task);
		}
	}

	*workflow_terminated = workflow_ended();

	if(tasks_index>0)
		record_savepoint(); // XML has been modified (tasks status update from DB)
	
	Events::GetInstance()->Create("INSTANCE_STARTED",workflow_instance_id);
}

void WorkflowInstance::DebugResume(bool *workflow_terminated)
{
	QueuePool *qp = QueuePool::GetInstance();

	unique_lock<recursive_mutex> llock(lock);
	
	// Put instance back in executing status
	xmldoc->getDocumentElement().setAttribute("status","EXECUTING");
	
	// Clear statistics
	clear_statistics();
	
	// Look for tasks or job that have conditions waiting for a new evaluation (via evqWait() function)
	{
		unique_ptr<DOMXPathResult> res(xmldoc->evaluate("//*[(name() = 'job' or name() = 'task') and @status='WAITING']",xmldoc->getDocumentElement(),DOMXPathResult::SNAPSHOT_RESULT_TYPE));
		int i = 0;
		while(res->snapshotItem(i++))
		{
			waiting_nodes.push_back((DOMElement)res->getNodeValue());
			waiting_conditions++;
			update_job_statistics("waiting_conditions",1,(DOMElement)res->getNodeValue());
		}
	}
	
	// Look for TERMINATED tasks (in error) and relaunch them. Also requeue QUEUED tasks
	int tasks_index = 0;
	{
		unique_ptr<DOMXPathResult> tasks(xmldoc->evaluate("//task[(@status='TERMINATED' and @retval!=0) or @status='QUEUED']",xmldoc->getDocumentElement(),DOMXPathResult::SNAPSHOT_RESULT_TYPE));
		DOMElement task;

		while(tasks->snapshotItem(tasks_index++))
		{
			task = (DOMElement)tasks->getNodeValue();
			
			if(task.getAttribute("status")=="TERMINATED")
			{
				// Reset task attributes
				task.removeAttribute("retry_schedule_level");
				task.removeAttribute("retry_times");
				task.removeAttribute("retry_at");
				task.removeAttribute("progression");
				task.removeAttribute("error");
				task.removeAttribute("retval");
				task.removeAttribute("retval");
				task.removeAttribute("execution_time");
			}
			
			enqueue_task(task);
		}
	}

	*workflow_terminated = workflow_ended();

	if(tasks_index>0)
		record_savepoint(); // XML has been modified (tasks status update from DB)
	
	Events::GetInstance()->Create("INSTANCE_STARTED",workflow_instance_id);
}

void WorkflowInstance::Migrate(bool *workflow_terminated)
{
	unique_lock<recursive_mutex> llock(lock);

	// For EXECUTING tasks : since we are migrating from another node, they will never end, fake termination
	unique_ptr<DOMXPathResult> tasks(xmldoc->evaluate("//task[@status='QUEUED' or @status='EXECUTING' or @status='TERMINATED']",xmldoc->getDocumentElement(),DOMXPathResult::SNAPSHOT_RESULT_TYPE));
	DOMElement task;

	int tasks_index = 0;
	while(tasks->snapshotItem(tasks_index++))
	{
		task = (DOMElement)tasks->getNodeValue();

		if(task.getAttribute("status")=="QUEUED")
			enqueue_task(task);
		else if(task.getAttribute("status")=="EXECUTING")
		{
			// Fake task ending with generic error code
			running_tasks++; // Inc running_tasks before calling TaskStop
			update_job_statistics("running_tasks",1,task);
			TaskStop(task,-1,"Task migrated",0,0,workflow_terminated);
		}
		else if(task.getAttribute("status")=="TERMINATED")
		{
			if(task.getAttribute("retval")!="0")
				retry_task(task);
		}
	}

	*workflow_terminated = workflow_ended();

	if(tasks_index>0)
		record_savepoint(); // XML has been modified (tasks status update from DB)

	// Workflow is migrated, update node name
	Configuration *config = ConfigurationEvQueue::GetInstance();

	DB db;
	db.QueryPrintf("UPDATE t_workflow_instance SET node_name=%s WHERE workflow_instance_id=%i",{&config->Get("cluster.node.name"),&workflow_instance_id});
}

void WorkflowInstance::Cancel()
{
	Logger::Log(LOG_NOTICE,"[WID %d] Cancelling",workflow_instance_id);

	unique_lock<recursive_mutex> llock(lock);

	is_cancelling = true;
}

void WorkflowInstance::Shutdown(void)
{
	is_shutting_down = true;
}
