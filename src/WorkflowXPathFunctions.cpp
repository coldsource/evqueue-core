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

#include <WorkflowXPathFunctions.h>
#include <XPathTokens.h>
#include <Exception.h>
#include <DOMXPathResult.h>

#include <memory>

using namespace std;

Token *WorkflowXPathFunctions::evqGetWorkflowParameter(XPathEval::func_context context, const vector<Token *> &args)
{
	if(args.size()!=1)
		throw Exception("evqGetWorkflowParameter()","Expecting 1 parameter");
	
	unique_ptr<Token> ret(context.eval->Evaluate("/workflow/parameters/parameter[@name = '"+(string)(*args.at(0))+"']",context.current_context));
	return new TokenString((string)(*ret));
}

Token *WorkflowXPathFunctions::evqGetParentJob(XPathEval::func_context context, const vector<Token *> &args)
{
	if(args.size()>1)
		throw Exception("evqGetParentJob()","Expecting 0 or 1 parameter");
	
	int nparents = 0;
	if(args.size()==1)
		nparents = (int)(*args.at(0));
	
	DOMNode node = *((DOMNode *)context.custom_context);
	for(int i=0;i<nparents;i++)
	{
		if(node.getParentNode() && node.getParentNode().getParentNode())
			node = node.getParentNode().getParentNode();
		else
			throw Exception("evqGeParentJob()","Not enough parents");
	}
	
	return new TokenNodeList(node);
}

Token *WorkflowXPathFunctions::evqGetOutput(XPathEval::func_context context, const vector<Token *> &args)
{
	if(args.size()!=1)
		throw Exception("evqGetOutput()","Expecting 1 parameter");
	
	string task_name = (string)(*args.at(0));
	DOMNode context_node;
	if(context.left_context->nodes.size()>0)
		context_node = context.left_context->nodes.at(0);
	else

		context_node = *((DOMNode *)context.custom_context);
	
	return context.eval->Evaluate("tasks/task[@name='"+task_name+"']/output",context_node);
}