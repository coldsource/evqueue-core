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

#include <XPath/WorkflowXPathFunctions.h>
#include <XPath/XPathTokens.h>
#include <Exception/Exception.h>
#include <DOM/DOMXPathResult.h>
#include <DOM/DOMDocument.h>

#include <memory>

using namespace std;

Token *WorkflowXPathFunctions::evqGetWorkflowParameter(XPathEval::func_context context, const vector<Token *> &args)
{
	if(args.size()!=1)
		throw Exception("evqGetWorkflowParameter()","Expecting 1 parameter");
	
	unique_ptr<Token> ret(context.eval->Evaluate("/workflow/parameters/parameter[@name = '"+(string)(*args.at(0))+"']",*context.current_context));
	return new TokenString((string)(*ret));
}

Token *WorkflowXPathFunctions::evqGetJob(XPathEval::func_context context, const vector<Token *> &args)
{
	if(args.size()!=1)
		throw Exception("evqGetOutput()","Expecting 1 parameter");
	
	string job_name = (string)(*args.at(0));
	DOMNode context_node = *((DOMNode *)context.custom_context);
	
	return context.eval->Evaluate("//subjobs/job[@name='"+job_name+"']",context_node);
}

Token *WorkflowXPathFunctions::evqGetCurrentJob(XPathEval::func_context context, const vector<Token *> &args)
{
	if(args.size()!=0)
		throw Exception("evqGetCurrentJob()","Expecting 0 parameters");
	
	DOMNode node = *((DOMNode *)context.custom_context);
	return new TokenSeq(new TokenNode(node));
}

Token *WorkflowXPathFunctions::evqGetParentJob(XPathEval::func_context context, const vector<Token *> &args)
{
	if(args.size()>1)
		throw Exception("evqGetParentJob()","Expecting 0 or 1 parameter");
	
	DOMNode node = *((DOMNode *)context.custom_context);
	if(args.size()==0 || args.at(0)->GetType()==LIT_INT)
	{
		int nparents = 0;
		if(args.size()==1)
			nparents = (int)(*args.at(0));
		
		for(int i=0;i<nparents;i++)
		{
			if(node.getParentNode() && node.getParentNode().getParentNode())
				node = node.getParentNode().getParentNode();
			else
				throw Exception("evqGetParentJob()","Not enough parents");
		}
	}
	else if(args.at(0)->GetType()==LIT_STR)
	{
		string name = string(*args.at(0));
		while(true)
		{
			if(((DOMElement)node).hasAttribute("name") && ((DOMElement)node).getAttribute("name")==name)
				break;
			
			if(node.getParentNode() && node.getParentNode().getParentNode())
				node = node.getParentNode().getParentNode();
			else
				throw Exception("evqGetParentJob()","Could not find job '"+name+"' in parents");
		}
	}
	else
		throw Exception("evqGeParentJob()","Expecting integer or string parameter");
	
	return new TokenSeq(new TokenNode(node));
}

Token *WorkflowXPathFunctions::evqGetOutput(XPathEval::func_context context, const vector<Token *> &args)
{
	if(args.size()!=1)
		throw Exception("evqGetOutput()","Expecting 1 parameter");
	
	string task_path = (string)(*args.at(0));
	DOMNode context_node;
	if(context.left_context->items.size()>0)
		context_node = *context.left_context->items.at(0);
	else
		context_node = *((DOMNode *)context.custom_context);
	
	return context.eval->Evaluate("tasks/task[@path='"+task_path+"' or @name='"+task_path+"']/output",context_node);
}

Token *WorkflowXPathFunctions::evqGetInput(XPathEval::func_context context, const vector<Token *> &args)
{
	if(args.size()!=2)
		throw Exception("evqGetOutput()","Expecting 2 parameters");
	
	string task_path = (string)(*args.at(0));
	string input_name = (string)(*args.at(1));
	
	DOMNode context_node;
	if(context.left_context->items.size()>0)
		context_node = *context.left_context->items.at(0);
	else
		context_node = *((DOMNode *)context.custom_context);
	
	return context.eval->Evaluate("tasks/task[@path='"+task_path+"']/input[@name='"+input_name+"']",context_node);
}

Token *WorkflowXPathFunctions::evqGetContext(XPathEval::func_context context, const vector<Token *> &args)
{
	if(args.size()!=0)
		throw Exception("evqGetContext()","Expecting 0 parameters");
	
	DOMNode context_node;
	if(context.left_context->items.size()>0)
		context_node = *context.left_context->items.at(0);
	else
		context_node = *((DOMNode *)context.custom_context);
	
	Token *ret = context.eval->Evaluate("@context-id",context_node);
	string context_id = (string)(*ret);
	delete ret;
	
	return new TokenSeq(new TokenNode(context.eval->GetXMLDoc()->getNodeFromEvqID(context_id)));
}

Token *WorkflowXPathFunctions::evqWait(XPathEval::func_context context, const vector<Token *> &args)
{
	if(args.size()!=1)
		throw Exception("evqWait()","Expecting 1 parameter");
	
	bool *needs_wait = (bool *)context.custom_context;
	if((bool)(*args.at(0)))
	{
		*needs_wait = false;
		return new TokenBool(true);
	}
	
	*needs_wait = true;
	throw Exception("evqWait()","Waiting for condition to become true");
}
