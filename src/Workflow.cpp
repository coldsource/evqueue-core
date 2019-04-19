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
#include <Workflows.h>
#include <WorkflowParameters.h>
#include <Notifications.h>
#include <DB.h>
#include <Exception.h>
#include <XMLUtils.h>
#include <Logger.h>
#include <LoggerAPI.h>
#include <Sha1String.h>
#include <SocketQuerySAX2Handler.h>
#include <QueryResponse.h>
#include <WorkflowScheduler.h>
#include <Users.h>
#include <Git.h>
#include <User.h>
#include <XMLFormatter.h>
#include <global.h>
#include <base64.h>

#include <string.h>

#include <memory>

using namespace std;

extern string workflow_xsd_str;

Workflow::Workflow()
{
	workflow_id = -1;
}

Workflow::Workflow(DB *db,const string &workflow_name)
{
	db->QueryPrintf("SELECT workflow_id,workflow_name,workflow_xml, workflow_group, workflow_comment, workflow_bound, workflow_lastcommit FROM t_workflow WHERE workflow_name=%s",&workflow_name);
	
	if(!db->FetchRow())
		throw Exception("Workflow","Unknown Workflow");
	
	workflow_id = db->GetFieldInt(0);
	
	this->workflow_name = db->GetField(1);
	workflow_xml = db->GetField(2);
	group = db->GetField(3);
	comment = db->GetField(4);
	bound_schedule = db->GetFieldInt(5);
	lastcommit = db->GetField(6);
	
	db->QueryPrintf("SELECT notification_id FROM t_workflow_notification WHERE workflow_id=%i",&workflow_id);
	while(db->FetchRow())
		notifications.push_back(db->GetFieldInt(0));
}

void Workflow::CheckInputParameters(WorkflowParameters *parameters)
{
	// Load workflow XML
	unique_ptr<DOMDocument> xmldoc(DOMDocument::Parse(workflow_xml));
	
	string parameter_name;
	string parameter_value;
	int passed_parameters = 0;
	
	parameters->SeekStart();
	while(parameters->Get(parameter_name,parameter_value))
	{
		unique_ptr<DOMXPathResult> res(xmldoc->evaluate("parameters/parameter[@name = '"+parameter_name+"']",xmldoc->getDocumentElement(),DOMXPathResult::FIRST_RESULT_TYPE));
		if(!res->isNode())
			throw Exception("Workflow","Unknown parameter : "+parameter_name);
		
		passed_parameters++;
	}
	
	unique_ptr<DOMXPathResult> res(xmldoc->evaluate("count(parameters/parameter)",xmldoc->getDocumentElement(),DOMXPathResult::FIRST_RESULT_TYPE));
	int workflow_template_parameters = res->getIntegerValue();
	
	if(workflow_template_parameters!=passed_parameters)
	{
		char e[256];
		sprintf(e, "Invalid number of parameters. Workflow expects %d, but %d are given.",workflow_template_parameters,passed_parameters);
		throw Exception("Workflow",e);
	}
}

bool Workflow::GetIsModified()
{
#ifndef USELIBGIT2
	return false;
#else
	// We are not bound to git, so we cannot compute this
	if(lastcommit=="")
		return false;
	
	// Check SHA1 between repo and current instance
	Git *git = Git::GetInstance();
	
	string repo_hash;
	try {
		repo_hash = git->GetWorkflowHash(lastcommit,workflow_name);
	} catch(Exception &e) {
		return true; // Errors can happen on repo change, ignore them
	}
	
	// Ensure uniform format
	XMLFormatter formatter(SaveToXML());
	formatter.Format(false);
	
	string db_hash = Sha1String(formatter.GetOutput()).GetBinary();
	
	return (repo_hash!=db_hash);
#endif
}

void Workflow::SetLastCommit(const std::string &commit_id)
{
	DB db;
	
	db.QueryPrintf("UPDATE t_workflow SET workflow_lastcommit=%s WHERE workflow_id=%i",commit_id.length()?&commit_id:0,&workflow_id);
}

string Workflow::SaveToXML()
{
	DOMDocument xmldoc;
	
	// Generate workflow XML
	DOMElement node = (DOMElement)XMLUtils::AppendXML(&xmldoc, xmldoc, "<workflow />");
	XMLUtils::AppendXML(&xmldoc, node, workflow_xml);
	node.setAttribute("group",group);
	node.setAttribute("comment",comment);
	
	return xmldoc.Serialize(xmldoc.getDocumentElement());
}

void Workflow::LoadFromXML(string name, DOMDocument *xmldoc, string repo_lastcommit)
{
	DOMElement root_node = xmldoc->getDocumentElement();
		
	string group = XMLUtils::GetAttribute(root_node, "group", true);
	string comment = XMLUtils::GetAttribute(root_node, "comment", true);
	
	string workflow_xml = xmldoc->Serialize(root_node.getFirstChild());
	
	string content;
	base64_encode_string(workflow_xml, content);
	
	if(Workflows::GetInstance()->Exists(name))
	{
		// Update
		Logger::Log(LOG_INFO,string("Updating workflow "+name+" from git").c_str());
		
		Workflow workflow = Workflows::GetInstance()->Get(name);
		
		Workflow::Edit(workflow.GetID(), name, content, group, comment);
		workflow.SetLastCommit(repo_lastcommit);
		
		Workflows::GetInstance()->Reload();
	}
	else
	{
		// Create
		Logger::Log(LOG_INFO,string("Crerating workflow "+name+" from git").c_str());
		
		unsigned int id = Workflow::Create(name, content, group, comment, repo_lastcommit);
		
		Workflows::GetInstance()->Reload();
	}
}

bool Workflow::CheckWorkflowName(const string &workflow_name)
{
	int i,len;
	
	len = workflow_name.length();
	if(len==0 || len>WORKFLOW_NAME_MAX_LEN)
		return false;
	
	for(i=0;i<len;i++)
		if(!isalnum(workflow_name[i]) && workflow_name[i]!='_' && workflow_name[i]!='-')
			return false;
	
	return true;
}

void Workflow::Get(unsigned int id, QueryResponse *response)
{
	Workflow workflow = Workflows::GetInstance()->Get(id);
	
	DOMElement node = (DOMElement)response->AppendXML("<workflow />");
	response->AppendXML(workflow.GetXML(), node);
	node.setAttribute("name",workflow.GetName());
	node.setAttribute("group",workflow.GetGroup());
	node.setAttribute("comment",workflow.GetComment());
}

unsigned int Workflow::Create(const string &name, const string &base64, const string &group, const string &comment, const string &lastcommit)
{
	string xml = create_edit_check(name,base64,group,comment);
	
	DB db;
	db.QueryPrintf("INSERT INTO t_workflow(workflow_name,workflow_xml,workflow_group,workflow_comment,workflow_lastcommit) VALUES(%s,%s,%s,%s,%s)",
		&name,
		&xml,
		&group,
		&comment,
		lastcommit.length()?&lastcommit:0
	);
	
	return db.InsertID();
}

void Workflow::Edit(unsigned int id,const string &name, const string &base64, const string &group, const string &comment)
{
	string xml = create_edit_check(name,base64,group,comment);
	
	if(!Workflows::GetInstance()->Exists(id))
		throw Exception("Workflow","Workflow not found");
	
	DB db;
	db.QueryPrintf("UPDATE t_workflow SET workflow_name=%s,workflow_xml=%s,workflow_group=%s,workflow_comment=%s WHERE workflow_id=%i",
		&name,
		&xml,
		&group,
		&comment,
		&id
	);
}

void Workflow::Delete(unsigned int id)
{
	DB db;
	
	bool schedule_deleted = false, rights_deleted = false;
	
	db.StartTransaction();
	
	db.QueryPrintf("DELETE FROM t_workflow WHERE workflow_id=%i",&id);
	
	if(db.AffectedRows()==0)
		throw Exception("Workflow","Workflow not found");
	
	// Clean notifications
	db.QueryPrintf("DELETE FROM t_workflow_notification WHERE workflow_id=%i",&id);
	
	// Clean schedules
	db.QueryPrintf("DELETE FROM t_workflow_schedule WHERE workflow_id=%i",&id);
	if(db.AffectedRows()>0)
		schedule_deleted = true;
	
	// Delete user rights associated
	db.QueryPrintf("DELETE FROM t_user_right WHERE workflow_id=%i",&id);
	if(db.AffectedRows()>0)
		rights_deleted = true;
	
	db.CommitTransaction();
	
	Workflows::GetInstance()->Reload();
	
	if(schedule_deleted)
		WorkflowScheduler::GetInstance()->Reload();
	
	if(rights_deleted)
		Users::GetInstance()->Reload();
}

void Workflow::ListNotifications(unsigned int id, QueryResponse *response)
{
	Workflow workflow = Workflows::GetInstance()->Get(id);
	
	for(int i=0;i<workflow.notifications.size();i++)
	{
		DOMElement node = (DOMElement)response->AppendXML("<notification />");
		node.setAttribute("id",to_string(workflow.notifications.at(i)));
	}
}

void Workflow::SubscribeNotification(unsigned int id, unsigned int notification_id)
{
	if(!Notifications::GetInstance()->Exists(notification_id))
		throw Exception("Workflow","Notification ID not found");
	
	DB db;
	db.QueryPrintf("INSERT INTO t_workflow_notification(workflow_id,notification_id) VALUES(%i,%i)",&id,&notification_id);
}

void Workflow::UnsubscribeNotification(unsigned int id, unsigned int notification_id)
{
	DB db;
	db.QueryPrintf("DELETE FROM t_workflow_notification WHERE workflow_id=%i AND notification_id=%i",&id,&notification_id);
	
	if(db.AffectedRows()==0)
		throw Exception("Workflow","Workflow was not subscribed to this notification");
}

void Workflow::ClearNotifications(unsigned int id)
{
	DB db;
	db.QueryPrintf("DELETE FROM t_workflow_notification WHERE workflow_id=%i",&id);
}

string Workflow::create_edit_check(const string &name, const string &base64, const string &group, const string &comment)
{
	if(name=="")
		throw Exception("Workflow","Workflow name cannot be empty");
	
	if(!CheckWorkflowName(name))
		throw Exception("Workflow","Invalid workflow name");
	
	string workflow_xml;
	if(!base64_decode_string(workflow_xml,base64))
		throw Exception("Workflow","Invalid base64 sequence");
	
	XMLUtils::ValidateXML(workflow_xml,workflow_xsd_str);
	
	return workflow_xml;
}

bool Workflow::HandleQuery(const User &user, SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	const string action = saxh->GetRootAttribute("action");
	
	if(action=="get")
	{
		unsigned int id = saxh->GetRootAttributeInt("id");
		
		Get(id,response);
		
		return true;
	}
	else if(action=="create" || action=="edit")
	{
		string name = saxh->GetRootAttribute("name");
		string content = saxh->GetRootAttribute("content");
		string group = saxh->GetRootAttribute("group","");
		string comment = saxh->GetRootAttribute("comment","");
		
		if(action=="create")
		{
			unsigned int id = Create(name, content, group, comment);
			
			LoggerAPI::LogAction(user,id,"Workflow",saxh->GetQueryGroup(),action);
			
			response->GetDOM()->getDocumentElement().setAttribute("workflow-id",to_string(id));
		}
		else
		{
			unsigned int id = saxh->GetRootAttributeInt("id");
			
			Edit(id, name, content, group, comment);
			
			LoggerAPI::LogAction(user,id,"Workflow",saxh->GetQueryGroup(),action);
		}
		
		Workflows::GetInstance()->Reload();
		
		return true;
	}
	else if(action=="delete")
	{
		unsigned int id = saxh->GetRootAttributeInt("id");
		
		Delete(id);
		
		LoggerAPI::LogAction(user,id,"Workflow",saxh->GetQueryGroup(),action);
		
		return true;
	}
	else if(action=="subscribe_notification" || action=="unsubscribe_notification" || action=="clear_notifications" || action=="list_notifications")
	{
		unsigned int id = saxh->GetRootAttributeInt("id");
		
		if(action=="clear_notifications")
		{
			ClearNotifications(id);
			
			LoggerAPI::LogAction(user,id,"Workflow",saxh->GetQueryGroup(),action);
			
			Workflows::GetInstance()->Reload();
			
			return true;
		}
		else if(action=="list_notifications")
		{
			ListNotifications(id, response);
			
			return true;
		}
		else if(action=="subscribe_notification" || action=="unsubscribe_notification")
		{
			unsigned int notification_id = saxh->GetRootAttributeInt("notification_id");
						
			if(action=="subscribe_notification")
				SubscribeNotification(id,notification_id);
			else
				UnsubscribeNotification(id,notification_id);
			
			LoggerAPI::LogAction(user,id,"Workflow",saxh->GetQueryGroup(),action);
			
			Workflows::GetInstance()->Reload();
			
			return true;
		}
	}
	
	return false;
}

string Workflow::CreateSimpleWorkflow(const string &task_name, const vector<std::string> &inputs)
{
	DOMDocument xmldoc;
	
	DOMElement workflow_node = xmldoc.createElement("workflow");
	xmldoc.appendChild(workflow_node);
	
	DOMElement parameters_node = xmldoc.createElement("parameters");
	workflow_node.appendChild(parameters_node);
	
	DOMElement subjobs_node = xmldoc.createElement("subjobs");
	workflow_node.appendChild(subjobs_node);
	
	DOMElement job_node = xmldoc.createElement("job");
	subjobs_node.appendChild(job_node);
	
	DOMElement tasks_node = xmldoc.createElement("tasks");
	job_node.appendChild(tasks_node);
	
	DOMElement task_node = xmldoc.createElement("task");
	task_node.setAttribute("name",task_name);
	task_node.setAttribute("queue","default");
	tasks_node.appendChild(task_node);
	
	for(int i = 0;i<inputs.size();i++)
	{
		DOMElement input_node = xmldoc.createElement("input");
		input_node.setAttribute("name",(string("argument_")+to_string(i)));
		input_node.appendChild(xmldoc.createTextNode(inputs.at(i)));
		task_node.appendChild(input_node);
	}
	
	return xmldoc.Serialize(workflow_node);
}
