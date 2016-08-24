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

#include <Git.h>
#include <LibGit2.h>
#include <SocketQuerySAX2Handler.h>
#include <QueryResponse.h>
#include <XMLUtils.h>
#include <Workflow.h>
#include <Workflows.h>
#include <Configuration.h>
#include <Exception.h>
#include <base64.h>

#include <xqilla/xqilla-dom3.hpp>

#include <stdio.h>
#include <string.h>

#include <string>

using namespace std;

Git *Git::instance = 0;

Git::Git()
{
	pthread_mutex_init(&lock, NULL);
	
	Configuration *config = Configuration::GetInstance();
	
	repo_path = config->Get("git.repository");
	
	if(repo_path.length())
		repo = new LibGit2(repo_path);
	
	instance = this;
}

Git::~Git()
{
	if(repo)
		delete repo;
}

void Git::SaveWorkflow(unsigned int id, const string &commit_log)
{
	if(!repo)
		throw Exception("Git", "No git repository is configured, this feature is disabled");
	
	pthread_mutex_lock(&lock);
	
	DOMDocument *xmldoc = 0;
	DOMLSSerializer *serializer = 0;
	XMLCh *workflow_xml = 0;
	char *workflow_xml_c = 0;
	
	try
	{
		// Get workflow form its ID
		Workflow workflow = Workflows::GetInstance()->Get(id);
		
		// Generate workflow XML
		DOMImplementation *xqillaImplementation = DOMImplementationRegistry::getDOMImplementation(X("XPath2 3.0"));
		xmldoc = xqillaImplementation->createDocument();
		
		DOMElement *node = (DOMElement *)XMLUtils::AppendXML(xmldoc, (DOMNode *)xmldoc, workflow.GetXML());
		node->setAttribute(X("name"),X(workflow.GetName().c_str()));
		node->setAttribute(X("group"),X(workflow.GetGroup().c_str()));
		node->setAttribute(X("comment"),X(workflow.GetComment().c_str()));
		
		serializer = xqillaImplementation->createLSSerializer();
		workflow_xml = serializer->writeToString(node);
		workflow_xml_c = XMLString::transcode(workflow_xml);
		
		
		// Write file to repo
		string filename = repo_path+"/"+workflow.GetName()+".xml";
		
		FILE *f = fopen(filename.c_str(), "w");
		if(!f)
			throw Exception("Git","Could not open file "+filename+" for writing");
		
		int workflow_len = strlen(workflow_xml_c);
		if(fwrite(workflow_xml_c, 1, workflow_len, f)!=workflow_len)
		{
			fclose(f);
			throw Exception("Git","Error writing workflow file");
		}
		
		fclose(f);
		
		// Commit + Push
		repo->AddFile(workflow.GetName()+".xml");
		repo->Commit(commit_log);
		repo->Push();
	}
	catch(Exception  &e)
	{
		if(workflow_xml)
			XMLString::release(&workflow_xml);
		
		if(workflow_xml_c)
			XMLString::release(&workflow_xml_c);
		
		if(serializer)
			serializer->release();
		
		if(xmldoc)
			xmldoc->release();
		
		pthread_mutex_unlock(&lock);
		
		throw e;
	}
	
	XMLString::release(&workflow_xml);
	XMLString::release(&workflow_xml_c);
	serializer->release();
	xmldoc->release();
	
	pthread_mutex_unlock(&lock);
}

void Git::LoadWorkflow(const string &name)
{
	if(!repo)
		throw Exception("Git", "No git repository is configured, this feature is disabled");
	
	pthread_mutex_lock(&lock);
	
	DOMLSParser *parser = 0;
	DOMLSSerializer *serializer = 0;
	
	try
	{
		DOMImplementation *xqillaImplementation = DOMImplementationRegistry::getDOMImplementation(X("XPath2 3.0"));
		parser = xqillaImplementation->createLSParser(DOMImplementationLS::MODE_SYNCHRONOUS,0);
		
		// Load XML from file
		string filename = repo_path+"/"+name+".xml";
		
		DOMDocument *xmldoc = parser->parseURI(filename.c_str());
		if(!xmldoc)
			throw Exception("Git", "Could not parse XML file "+filename);
		
		DOMElement *root_node = xmldoc->getDocumentElement();
		
		string name = XMLUtils::GetAttribute(root_node, "name", true);
		string group = XMLUtils::GetAttribute(root_node, "group", true);
		string comment = XMLUtils::GetAttribute(root_node, "comment", true);
		
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
			Workflow workflow = Workflows::GetInstance()->Get(name);
			
			Workflow::Edit(workflow.GetID(), name, content, group, comment);
		}
		else
		{
			// Create
			Workflow::Create(name, content, group, comment);
		}
	}
	catch(Exception &e)
	{
		if(parser)
			parser->release();
		
		if(serializer)
			serializer->release();
		
		pthread_mutex_unlock(&lock);
		
		throw e;
	}
	
	parser->release();
	serializer->release();
	
	pthread_mutex_unlock(&lock);
}

bool Git::HandleQuery(SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	const string action = saxh->GetRootAttribute("action");
	
	if(action=="save")
	{
		unsigned int id = saxh->GetRootAttributeInt("id");
		string commit_log = saxh->GetRootAttribute("commit_log");
		
		Git::GetInstance()->SaveWorkflow(id,commit_log);
		
		return true;
	}
	else if(action=="load")
	{
		string name = saxh->GetRootAttribute("name");
		
		Git::GetInstance()->LoadWorkflow(name);
		
		return true;
	}
	
	return false;
}