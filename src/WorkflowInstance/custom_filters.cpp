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
#include <XPath/WorkflowXPathFunctions.h>
#include <Logger/Logger.h>
#include <DB/DB.h>

#include <unistd.h>

#include <memory>

using namespace std;

void WorkflowInstance::fill_custom_filters()
{
	DB db;
	
	
	DOMElement root = xmldoc->getDocumentElement();
	xmldoc->getXPath()->RegisterFunction("evqGetJob",{WorkflowXPathFunctions::evqGetJob,&root});
	
	try
	{
		unique_ptr<DOMXPathResult> filters(xmldoc->evaluate("/workflow/custom-attributes/custom-attribute",xmldoc->getDocumentElement(),DOMXPathResult::SNAPSHOT_RESULT_TYPE));
		int filters_index = 0;
		while(filters->snapshotItem(filters_index++))
		{
			DOMElement filter = (DOMElement)filters->getNodeValue();

			// This is unchecked user input. We have to try evaluation
			string name = filter.getAttribute("name");
			string select = filter.getAttribute("select");
			unique_ptr<DOMXPathResult> value_nodes(xmldoc->evaluate(select,xmldoc->getDocumentElement(),DOMXPathResult::SNAPSHOT_RESULT_TYPE));
			
			int value_nodes_index = 0;
			while(value_nodes->snapshotItem(value_nodes_index++))
			{
				DOMElement value_node = (DOMElement)value_nodes->getNodeValue();
				string value = value_node.getTextContent();
				
				db.QueryPrintf(
					"INSERT INTO t_workflow_instance_filters(workflow_instance_id,workflow_instance_filter,workflow_instance_filter_value) VALUES(%i,%s,%s)",
					&workflow_instance_id,&name,&value
				);
			}
		}
	}
	catch(Exception &e)
	{
		Logger::Log(LOG_WARNING,"[WID "+to_string(workflow_instance_id)+"] Could not evaluate custom filters : "+e.error);
	}
}
