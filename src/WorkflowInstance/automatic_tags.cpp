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
#include <WS/Events.h>
#include <Tag/Tag.h>
#include <Tag/Tags.h>

#include <unistd.h>

#include <memory>

using namespace std;

void WorkflowInstance::fill_automatic_tags()
{
	DB db;
	
	
	DOMElement root = xmldoc->getDocumentElement();
	xmldoc->getXPath()->RegisterFunction("evqGetJob",{WorkflowXPathFunctions::evqGetJob,&root});
	
	try
	{
		unique_ptr<DOMXPathResult> filters(xmldoc->evaluate("/workflow/automatic-tags/automatic-tag",xmldoc->getDocumentElement(),DOMXPathResult::SNAPSHOT_RESULT_TYPE));
		int filters_index = 0;
		while(filters->snapshotItem(filters_index++))
		{
			DOMElement filter = (DOMElement)filters->getNodeValue();

			// This is unchecked user input. We have to try evaluation
			string name = filter.getAttribute("name");
			string condition = filter.getAttribute("condition");
			unique_ptr<DOMXPathResult> test_expr(xmldoc->evaluate(condition,xmldoc->getDocumentElement(),DOMXPathResult::BOOLEAN_TYPE));
			
			if(test_expr->getBooleanValue())
			{
				unsigned int tag_id = Tag::Create(name);
				
				db.QueryPrintf(
					"INSERT INTO t_workflow_instance_tag(workflow_instance_id,tag_id) VALUES(%i,%i)",
					&workflow_instance_id,&tag_id
				);
				
				Events::GetInstance()->Create(Events::en_types::INSTANCE_TAGGED,workflow_instance_id);
			}
		}
	}
	catch(Exception &e)
	{
		Logger::Log(LOG_WARNING,"[WID "+to_string(workflow_instance_id)+"] Could not process automatic tagging : "+e.error);
	}
}
