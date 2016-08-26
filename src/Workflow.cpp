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
#include <Sha1String.h>
#include <SocketQuerySAX2Handler.h>
#include <QueryResponse.h>
#include <WorkflowScheduler.h>
#include <Users.h>
#include <Tasks.h>
#include <Git.h>
#include <User.h>
#include <global.h>
#include <base64.h>

#include <string.h>

#include <xqilla/xqilla-dom3.hpp>

using namespace std;
using namespace xercesc;

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
	lastcommit = db->GetField(6)?db->GetField(6):"";
	
	db->QueryPrintf("SELECT COUNT(*) FROM t_task WHERE workflow_id = %i",&workflow_id);
	db->FetchRow();
	bound_task = (db->GetFieldInt(0) > 0)?true:false;
	
	db->QueryPrintf("SELECT notification_id FROM t_workflow_notification WHERE workflow_id=%i",&workflow_id);
	while(db->FetchRow())
		notifications.push_back(db->GetFieldInt(0));
}

void Workflow::CheckInputParameters(WorkflowParameters *parameters)
{
	// Load workflow XML
	DOMImplementation *xqillaImplementation = DOMImplementationRegistry::getDOMImplementation(X("XPath2 3.0"));
	DOMLSParser *parser = xqillaImplementation->createLSParser(DOMImplementationLS::MODE_SYNCHRONOUS,0);
	DOMLSSerializer *serializer = xqillaImplementation->createLSSerializer();
	
	DOMLSInput *input = xqillaImplementation->createLSInput();
	
	// Set XML content and parse document
	XMLCh *xml;
	xml = XMLString::transcode(workflow_xml.c_str());
	input->setStringData(xml);
	DOMDocument *xmldoc = parser->parse(input);
	
	input->release();
	
	XMLString::release(&xml);
	
	DOMXPathNSResolver *resolver = xmldoc->createNSResolver(xmldoc->getDocumentElement());
	resolver->addNamespaceBinding(X("xs"), X("http://www.w3.org/2001/XMLSchema"));
	
	try
	{
		string parameter_name;
		string parameter_value;
		int passed_parameters = 0;
		char buf2[256+PARAMETER_NAME_MAX_LEN];
		
		parameters->SeekStart();
		while(parameters->Get(parameter_name,parameter_value))
		{
			sprintf(buf2,"parameters/parameter[@name = '%s']",parameter_name.c_str());
			DOMXPathResult *res = xmldoc->evaluate(X(buf2),xmldoc->getDocumentElement(),resolver,DOMXPathResult::FIRST_RESULT_TYPE,0);
			if(!res->isNode())
			{
				res->release();
				throw Exception("Workflow","Unknown parameter : "+parameter_name);
			}
			
			res->release();
			passed_parameters++;
		}
		
		DOMXPathResult *res = xmldoc->evaluate(X("count(parameters/parameter)"),xmldoc->getDocumentElement(),resolver,DOMXPathResult::FIRST_RESULT_TYPE,0);
		int workflow_template_parameters = res->getIntegerValue();
		res->release();
		
		if(workflow_template_parameters!=passed_parameters)
		{
			char e[256];
			sprintf(e, "Invalid number of parameters. Workflow expects %d, but %d are given.",workflow_template_parameters,passed_parameters);
			throw Exception("Workflow",e);
		}
	}
	catch(Exception &e)
	{
		parser->release();
		serializer->release();
		
		throw e;
	}
	
	parser->release();
	serializer->release();
}

bool Workflow::GetIsModified()
{
	// We are not bound to git, so we cannot compute this
	if(lastcommit=="")
		return false;
	
	// Check SHA1 between repo and current instance
	Git *git = Git::GetInstance();
	
	string repo_hash = git->GetWorkflowHash(workflow_name);
	
	string db_hash = Sha1String(SaveToXML()).GetBinary();
	
	return (repo_hash!=db_hash);
}

void Workflow::SetLastCommit(const std::string &commit_id)
{
	DB db;
	
	db.QueryPrintf("UPDATE t_workflow SET workflow_lastcommit=%s WHERE workflow_id=%i",commit_id.length()?&commit_id:0,&workflow_id);
}

string Workflow::SaveToXML()
{
	DOMDocument *xmldoc = 0;
	DOMLSSerializer *serializer = 0;
	
	// Generate workflow XML
	DOMImplementation *xqillaImplementation = DOMImplementationRegistry::getDOMImplementation(X("XPath2 3.0"));
	xmldoc = xqillaImplementation->createDocument();
	
	DOMElement *node = (DOMElement *)XMLUtils::AppendXML(xmldoc, (DOMNode *)xmldoc, workflow_xml);
	node->setAttribute(X("group"),X(group.c_str()));
	node->setAttribute(X("comment"),X(comment.c_str()));
	
	serializer = xqillaImplementation->createLSSerializer();
	XMLCh *workflow_xml = serializer->writeToString(node);
	char *workflow_xml_c = XMLString::transcode(workflow_xml);
	string ret_xml = workflow_xml_c;
	
	XMLString::release(&workflow_xml);
	XMLString::release(&workflow_xml_c);
	serializer->release();
	xmldoc->release();
	
	return ret_xml;
}

void Workflow::LoadFromXML(string name, DOMDocument *xmldoc, string repo_lastcommit)
{
	DOMLSSerializer *serializer = 0;
	
	DOMElement *root_node = xmldoc->getDocumentElement();
		
	string group = XMLUtils::GetAttribute(root_node, "group", true);
	string comment = XMLUtils::GetAttribute(root_node, "comment", true);
	
	try
	{
		DOMImplementation *xqillaImplementation = DOMImplementationRegistry::getDOMImplementation(X("XPath2 3.0"));
		serializer = xqillaImplementation->createLSSerializer();
		XMLCh *workflow_xml = serializer->writeToString(root_node);
		char *workflow_xml_c = XMLString::transcode(workflow_xml);
		
		string content;
		base64_encode_string(workflow_xml_c, content);
		
		XMLString::release(&workflow_xml);
		XMLString::release(&workflow_xml_c);
		
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
	catch(Exception &e)
	{
		if(serializer)
			serializer->release();
		
		throw e;
	}
	
	serializer->release();
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
	
	DOMElement *node = (DOMElement *)response->AppendXML(workflow.GetXML());
	node->setAttribute(X("name"),X(workflow.GetName().c_str()));
	node->setAttribute(X("group"),X(workflow.GetGroup().c_str()));
	node->setAttribute(X("comment"),X(workflow.GetComment().c_str()));
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
	
	bool task_deleted = false, schedule_deleted = false, rights_deleted = false;
	
	db.StartTransaction();
	
	db.QueryPrintf("DELETE FROM t_workflow WHERE workflow_id=%i",&id);
	
	if(db.AffectedRows()==0)
		throw Exception("Workflow","Workflow not found");
	
	// Also delete associated tasks
	db.QueryPrintf("DELETE FROM t_task WHERE workflow_id=%i",&id);
	if(db.AffectedRows()==0)
		task_deleted = false;
	
	// Clean notifications
	db.QueryPrintf("DELETE FROM t_workflow_notification WHERE workflow_id=%i",&id);
	
	// Clean schedules
	db.QueryPrintf("DELETE FROM t_workflow_schedule WHERE workflow_id=%i",&id);
	if(db.AffectedRows()==0)
		schedule_deleted = false;
	
	// Delete user rights associated
	db.QueryPrintf("DELETE FROM t_user_right WHERE workflow_id=%i",&id);
	if(db.AffectedRows()==0)
		rights_deleted = false;
	
	db.CommitTransaction();
	
	Workflows::GetInstance()->Reload();
	
	if(task_deleted)
		Tasks::GetInstance()->Reload();
	
	if(schedule_deleted)
		WorkflowScheduler::GetInstance()->Reload();
	
	if(rights_deleted)
		Users::GetInstance()->Reload();
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
			Create(name, content, group, comment);
		else
		{
			unsigned int id = saxh->GetRootAttributeInt("id");
			
			Edit(id, name, content, group, comment);
		}
		
		Workflows::GetInstance()->Reload();
		
		return true;
	}
	else if(action=="delete")
	{
		unsigned int id = saxh->GetRootAttributeInt("id");
		
		Delete(id);
		
		return true;
	}
	else if(action=="subscribe_notification" || action=="unsubscribe_notification" || action=="clear_notifications")
	{
		unsigned int id = saxh->GetRootAttributeInt("id");
		
		if(action=="clear_notifications")
		{
			ClearNotifications(id);
			
			Workflows::GetInstance()->Reload();
			
			return true;
		}
		else if(action=="subscribe_notification" || action=="unsubscribe_notification")
		{
			unsigned int notification_id = saxh->GetRootAttributeInt("notification_id");
						
			if(action=="subscribe_notification")
				SubscribeNotification(id,notification_id);
			else
				UnsubscribeNotification(id,notification_id);
			
			Workflows::GetInstance()->Reload();
			
			return true;
		}
	}
	
	return false;
}

string Workflow::CreateSimpleWorkflow(const string &task_name, const vector<std::string> &inputs)
{
	DOMImplementation *xqillaImplementation = DOMImplementationRegistry::getDOMImplementation(X("XPath2 3.0"));
	DOMDocument *xmldoc = xqillaImplementation->createDocument();
	
	DOMElement *workflow_node = xmldoc->createElement(X("workflow"));
	xmldoc->appendChild(workflow_node);
	
	DOMElement *parameters_node = xmldoc->createElement(X("parameters"));
	workflow_node->appendChild(parameters_node);
	
	DOMElement *subjobs_node = xmldoc->createElement(X("subjobs"));
	workflow_node->appendChild(subjobs_node);
	
	DOMElement *job_node = xmldoc->createElement(X("job"));
	subjobs_node->appendChild(job_node);
	
	DOMElement *tasks_node = xmldoc->createElement(X("tasks"));
	job_node->appendChild(tasks_node);
	
	DOMElement *task_node = xmldoc->createElement(X("task"));
	task_node->setAttribute(X("name"),X(task_name.c_str()));
	task_node->setAttribute(X("queue"),X("default"));
	tasks_node->appendChild(task_node);
	
	for(int i = 0;i<inputs.size();i++)
	{
		DOMElement *input_node = xmldoc->createElement(X("input"));
		input_node->setAttribute(X("name"),X((string("argument_")+to_string(i)).c_str()));
		input_node->appendChild(xmldoc->createTextNode(X(inputs.at(i).c_str())));
		task_node->appendChild(input_node);
	}
	
	DOMLSSerializer *serializer = xqillaImplementation->createLSSerializer();
	XMLCh *workflow_xml = serializer->writeToString(workflow_node);
	char *workflow_xml_c = XMLString::transcode(workflow_xml);
	
	string workflow_xml_str = workflow_xml_c;
	
	XMLString::release(&workflow_xml);
	XMLString::release(&workflow_xml_c);
	serializer->release();
	
	xmldoc->release();
	
	return workflow_xml_str;
}