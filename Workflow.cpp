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

#include <Workflow.h>
#include <DB.h>
#include <Exception.h>

#include <string.h>

Workflow::Workflow()
{
	workflow_id = -1;
	workflow_name = 0;
	workflow_xml = 0;
}

Workflow::Workflow(DB *db,const char *workflow_name)
{
	db->QueryPrintf("SELECT workflow_id,workflow_name,workflow_xml FROM t_workflow WHERE workflow_name=%s",workflow_name);
	
	if(!db->FetchRow())
		throw Exception("Workflow","Unknown Workflow");
	
	workflow_id = db->GetFieldInt(0);
	
	this->workflow_name = new char[strlen(db->GetField(1))+1];
	strcpy(this->workflow_name,db->GetField(1));
	
	workflow_xml = new char[strlen(db->GetField(2))+1];
	strcpy(workflow_xml,db->GetField(2));
}

Workflow::Workflow(const Workflow &workflow)
{
	workflow_id = workflow.workflow_id;
	
	workflow_name = new char[strlen(workflow.workflow_name)+1];
	strcpy(workflow_name,workflow.workflow_name);
	
	workflow_xml = new char[strlen(workflow.workflow_xml)+1];
	strcpy(workflow_xml,workflow.workflow_xml);
}

Workflow::~Workflow()
{
	free();
}

Workflow &Workflow::operator=(const Workflow &workflow)
{
	free();
	
	workflow_id = workflow.workflow_id;
	
	workflow_name = new char[strlen(workflow.workflow_name)+1];
	strcpy(workflow_name,workflow.workflow_name);
	
	workflow_xml = new char[strlen(workflow.workflow_xml)+1];
	strcpy(workflow_xml,workflow.workflow_xml);
}

void Workflow::free(void)
{
	if(workflow_name)
	{
		delete[] workflow_name;
		workflow_name = 0;
	}
	
	if(workflow_xml)
	{
		delete[] workflow_xml;
		workflow_xml = 0;
	}
}
