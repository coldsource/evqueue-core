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

#include <XPathEval.h>
#include <XPathTokens.h>
#include <XPathOperators.h>
#include <XPathFunctions.h>
#include <DOMDocument.h>
#include <DOMNamedNodeMap.h>
#include <DOMNode.h>
#include <Exception.h>

#include <memory>

using namespace std;

// Return filtered child nodes of context node ('node name' operator)
TokenNodeList *XPathEval::get_child_nodes(string name,DOMNode context,TokenNodeList *node_list)
{
	DOMNode node = context.getFirstChild();
	
	if(node_list==0)
		node_list = new TokenNodeList();
	
	if(name==".")
	{
		node_list->nodes.push_back(context);
		return node_list;
	}
	else if(name=="..")
	{
		DOMNode parent = context.getParentNode();
		if(parent)
			node_list->nodes.push_back(parent);
		
		return node_list;
	}
	
	while(node)
	{
		if(node.getNodeType()==DOMNode::ELEMENT_NODE && (node.getNodeName()==name || name=="*"))
			node_list->nodes.push_back(node);
		node = node.getNextSibling();
	}
	
	return node_list;
}

// Same as preceding function but on node list (the / operator)
TokenNodeList *XPathEval::get_child_nodes(string name,TokenNodeList *context,TokenNodeList *node_list)
{
	if(node_list==0)
		node_list = new TokenNodeList();
	
	for(int i=0;i<context->nodes.size();i++)
		get_child_nodes(name,context->nodes.at(i),node_list);
	
	return node_list;
}

// Return filtered attribute names of context node (the 'attribute name' operator)
TokenNodeList *XPathEval::get_child_attributes(string name,DOMNode context,TokenNodeList *node_list)
{
	DOMNamedNodeMap map = context.getAttributes();
	
	if(node_list==0)
		node_list = new TokenNodeList();
	
	for(int i=0;i<map.getLength();i++)
	{
		if(name=="*" || map.item(i).getNodeName()==name)
			node_list->nodes.push_back((DOMNode)map.item(i));
	}
	
	return node_list;
}

// Same as preceding function but on node list (the / operator between node name and attribute name)
TokenNodeList *XPathEval::get_child_attributes(string name,TokenNodeList *context,TokenNodeList *node_list)
{
	if(node_list==0)
		node_list = new TokenNodeList();
	
	for(int i=0;i<context->nodes.size();i++)
		get_child_attributes(name,context->nodes.at(i),node_list);
	
	return node_list;
}

// The // operator
TokenNodeList *XPathEval::get_all_child_nodes(string name,DOMNode context,TokenNodeList *node_list)
{
	DOMNode node = context.getFirstChild();
	
	if(node_list==0)
		node_list = new TokenNodeList();
	
	while(node)
	{
		get_all_child_nodes(name,node,node_list);
			
		if(node.getNodeType()==DOMNode::ELEMENT_NODE && (node.getNodeName()==name || name=="*"))
			node_list->nodes.push_back(node);
		node = node.getNextSibling();
	}
	
	return node_list;
}

// The // operator
TokenNodeList *XPathEval::get_all_child_nodes(string name,TokenNodeList *context,TokenNodeList *node_list)
{
	if(node_list==0)
		node_list = new TokenNodeList();
	
	for(int i=0;i<context->nodes.size();i++)
		get_all_child_nodes(name,context->nodes.at(i),node_list);
	
	return node_list;
}

// Evaluate a node filter to see if it must be keept or node
// Xpath syntax : /node[<filter expression>]
void XPathEval::filter_token_node_list(TokenNodeList *list,TokenExpr *filter,DOMDocument *xmldoc,DOMNode context)
{
	for(int i=0;i<list->nodes.size();i++)
	{
		TokenExpr *filter_copy = new TokenExpr(*filter);
		Token *token = evaluate_expr(filter_copy,xmldoc,list->nodes.at(i));
		if(!(bool)(*token))
		{
			list->nodes.erase(list->nodes.begin()+i);
			i--;
		}
		delete token;
	}
}

// Get nth node from a node list
// Xpath syntax : /node[<integer>]
void XPathEval::get_nth_token_node_list(TokenNodeList *list,int n,DOMDocument *xmldoc,DOMNode context)
{
	for(int i=0;i<list->nodes.size();i++)
	{
		if(i!=n-1)
		{
			list->nodes.erase(list->nodes.begin()+i);
			i--;
		}
	}
}

// Evaluate a fully parsed expression
Token *XPathEval::evaluate_expr(Token *token,DOMDocument *xmldoc,DOMNode context)
{
	if(token->GetType()!=EXPR)
		return token; // Nothing to do
	
	TokenExpr *expr = (TokenExpr *)token;
	
	// Resolve expressions, functions, node names and attribute names
	for(int i=0;i<expr->expr_tokens.size();i++)
	{
		TOKEN_TYPE token_type = expr->expr_tokens.at(i)->GetType();
		Token *val = 0;
		int replace_from, replace_to;
		
		if(token_type==EXPR)
		{
			// Recursively parse expressions
			val = evaluate_expr(expr->expr_tokens.at(i),xmldoc,context);
			replace_from = replace_to = i;
		}
		else if(token_type==FUNC)
		{
			// Lookup funcion name
			TokenFunc *func = (TokenFunc *)expr->expr_tokens.at(i);
			auto it = funcs_desc.find(func->name);
			if(it==funcs_desc.end())
				throw Exception("XPath","Unknown function : "+func->name);
			
			vector<Token *>args;
			try
			{
				// Evaluate function parameters
				for(int j=0;j<func->args.size();)
				{
					args.push_back(evaluate_expr(func->args.at(j),xmldoc,context));
					func->args.erase(func->args.begin());
				}
				
				// Prepare context
				TokenNodeList lcontext(context);
				
				// Call function implementation
				val = it->second.impl({&lcontext,it->second.custom_context,this},args);
			}
			catch(Exception &e)
			{
				for(int j=0;j<args.size();j++)
					delete args.at(j);
				throw Exception("XPath",e.context+" : "+e.error);
			}
			
			for(int j=0;j<args.size();j++)
				delete args.at(j);
			
			replace_from = replace_to = i;
		}
		else if(token_type==NODENAME)
		{
			// Single node name (not precedeed by / or //). Compute based on current context (relative path)
			TokenNodeName *node_name = (TokenNodeName *)expr->expr_tokens.at(i);
			
			val = get_child_nodes(node_name->name,context,0);
			
			// Apply node filter if present
			if(node_name->filter)
			{
				if(((TokenExpr *)node_name->filter)->expr_tokens.size()==1 && ((TokenExpr *)node_name->filter)->expr_tokens.at(0)->GetType()==LIT_INT)
					get_nth_token_node_list((TokenNodeList *)val,((TokenInt *)((TokenExpr *)node_name->filter)->expr_tokens.at(0))->i,xmldoc,context);
				else
					filter_token_node_list((TokenNodeList *)val,node_name->filter,xmldoc,context);
			}
			
			replace_from = replace_to = i;
		}
		else if(token_type==ATTRNAME)
		{
			// Single attribute name (not precedeed by / or //). Compute based on current context (relative path)
			TokenAttrName *attr_name = (TokenAttrName *)expr->expr_tokens.at(i);
			
			val = get_child_attributes(attr_name->name,context,0);
			replace_from = replace_to = i;
		}
		else if(token_type==SLASH)
		{
			// Node name precedeed by '/'. Compute based on preceding node list.
			// XPath syntext : node1/node2 (node 1 has already been resolved to DOM node list)
			if(i+1<expr->expr_tokens.size() && expr->expr_tokens.at(i+1)->GetType()==NODENAME)
			{
				TokenNodeName *node_name = (TokenNodeName *)expr->expr_tokens.at(i+1);
				
				if(i>0 && expr->expr_tokens.at(i-1)->GetType()==NODELIST)
				{
					val = get_child_nodes(node_name->name,(TokenNodeList *)expr->expr_tokens.at(i-1),0);
					replace_from = i-1;
				}
				else
				{
					val = get_child_nodes(node_name->name,(DOMNode)(*xmldoc),0);
					replace_from = i;
				}
				
				// Apply node filter if present
				if(node_name->filter)
				{
					if(((TokenExpr *)node_name->filter)->expr_tokens.size()==1 && ((TokenExpr *)node_name->filter)->expr_tokens.at(0)->GetType()==LIT_INT)
						get_nth_token_node_list((TokenNodeList *)val,((TokenInt *)((TokenExpr *)node_name->filter)->expr_tokens.at(0))->i,xmldoc,context);
					else
						filter_token_node_list((TokenNodeList *)val,node_name->filter,xmldoc,context);
				}
			}
			// Same but for attribute name
			else if(i+1<expr->expr_tokens.size() && expr->expr_tokens.at(i+1)->GetType()==ATTRNAME)
			{
				TokenNodeName *node_name = (TokenNodeName *)expr->expr_tokens.at(i+1);
				
				if(i>0 && expr->expr_tokens.at(i-1)->GetType()==NODELIST)
				{
					val = get_child_attributes(node_name->name,(TokenNodeList *)expr->expr_tokens.at(i-1),0);
					replace_from = i-1;
				}
				else
				{
					val = get_child_attributes(node_name->name,(DOMNode)(*xmldoc),0);
					replace_from = i;
				}
			}
			else
				throw Exception("XPath","Missing node or attribute name after slash");
			
			replace_to = i+1;
		}
		else if(token_type==DSLASH)
		{
			// Same as / operator but for // (lookup for nodes in all childs)
			if(i+1>=expr->expr_tokens.size() || expr->expr_tokens.at(i+1)->GetType()!=NODENAME)
				throw Exception("XPath","Missing node name after dslash");
			
			TokenNodeName *node_name = (TokenNodeName *)expr->expr_tokens.at(i+1);
			
			if(i>0 && expr->expr_tokens.at(i-1)->GetType()==NODELIST)
			{
				val = get_all_child_nodes(node_name->name,(TokenNodeList *)expr->expr_tokens.at(i-1),0);
				replace_from = i-1;
			}
			else
			{
				val = get_all_child_nodes(node_name->name,(DOMNode)(*xmldoc),0);
				replace_from = i;
			}
			
			if(node_name->filter)
			{
				if(((TokenExpr *)node_name->filter)->expr_tokens.size()==1 && ((TokenExpr *)node_name->filter)->expr_tokens.at(0)->GetType()==LIT_INT)
					get_nth_token_node_list((TokenNodeList *)val,((TokenInt *)((TokenExpr *)node_name->filter)->expr_tokens.at(0))->i,xmldoc,context);
				else
					filter_token_node_list((TokenNodeList *)val,node_name->filter,xmldoc,context);
			}
			
			replace_to = i+1;
		}
		
		if(val)
		{
			if(token_type!=EXPR)
				for(int j=replace_from;j<=replace_to;j++)
					delete expr->expr_tokens.at(j);
			expr->expr_tokens.erase(expr->expr_tokens.begin()+replace_from,expr->expr_tokens.begin()+replace_to+1);
			expr->expr_tokens.insert(expr->expr_tokens.begin()+replace_from,val);
			i = replace_from;
		}
	}
	
	// Compute operators
	while(true)
	{
		// Find oprator with lower priority
		int minop = 500,minop_index = -1;
		for(int i=0;i<expr->expr_tokens.size();i++)
		{
			if(expr->expr_tokens.at(i)->GetType()==OP && ops_desc.at(((TokenOP *)expr->expr_tokens.at(i))->op).prio<minop)
			{
				minop = ops_desc.at(((TokenOP *)expr->expr_tokens.at(i))->op).prio;
				minop_index = i;
			}
		}
		
		if(minop_index==-1)
			break; // No operators left
		
		TokenOP *op = (TokenOP *)expr->expr_tokens.at(minop_index);
		
		// Get left operand
		if(minop_index-1<0)
			throw Exception("XPath","Missing left operand");
		Token *left = expr->expr_tokens.at(minop_index-1);
		
		//  Get right operand
		if(minop_index+1>=expr->expr_tokens.size())
			throw Exception("XPath","Missing right operand");
		Token *right = expr->expr_tokens.at(minop_index+1);
		
		// Compute operator result
		Token *new_token = ops_desc.at(op->op).impl(left,right);
		
		// Replace value in expression
		delete op;
		delete left;
		delete right;
		expr->expr_tokens.erase(expr->expr_tokens.begin()+minop_index-1,expr->expr_tokens.begin()+minop_index+2);
		expr->expr_tokens.insert(expr->expr_tokens.begin()+minop_index-1,new_token);
	}
	
	if(expr->expr_tokens.size()!=1)
		throw Exception("XPath","Error evaluating expression");
	
	Token *ret = expr->expr_tokens.at(0)->clone();
	delete expr;
	
	return ret;
}

XPathEval::XPathEval(DOMDocument *xmldoc)
{
	this->xmldoc = xmldoc;
	
	// Initialize operators
	ops_desc.insert(ops_desc.begin()+OPERATOR::MULT,{0,XPathOperators::Operator_MULT});
	ops_desc.insert(ops_desc.begin()+OPERATOR::DIV,{0,XPathOperators::Operator_DIV});
	ops_desc.insert(ops_desc.begin()+OPERATOR::MOD,{0,XPathOperators::Operator_MOD});
	ops_desc.insert(ops_desc.begin()+OPERATOR::PLUS,{1,XPathOperators::Operator_PLUS});
	ops_desc.insert(ops_desc.begin()+OPERATOR::MINUS,{1,XPathOperators::Operator_MINUS});
	ops_desc.insert(ops_desc.begin()+OPERATOR::LT,{2,XPathOperators::Operator_LT});
	ops_desc.insert(ops_desc.begin()+OPERATOR::LEQ,{2,XPathOperators::Operator_LEQ});
	ops_desc.insert(ops_desc.begin()+OPERATOR::GT,{2,XPathOperators::Operator_GT});
	ops_desc.insert(ops_desc.begin()+OPERATOR::GEQ,{2,XPathOperators::Operator_GEQ});
	ops_desc.insert(ops_desc.begin()+OPERATOR::EQ,{3,XPathOperators::Operator_EQ});
	ops_desc.insert(ops_desc.begin()+OPERATOR::NEQ,{3,XPathOperators::Operator_NEQ});
	ops_desc.insert(ops_desc.begin()+OPERATOR::AND,{4,XPathOperators::Operator_AND});
	ops_desc.insert(ops_desc.begin()+OPERATOR::OR,{5,XPathOperators::Operator_OR});
	
	// Initialize functions
	funcs_desc.insert(pair<string,func_desc>("count",{XPathFunctions::count,0}));
	funcs_desc.insert(pair<string,func_desc>("substring",{XPathFunctions::substring,0}));
}

void XPathEval::RegisterFunction(string name,func_desc f)
{
	funcs_desc.erase(name);
	funcs_desc.insert(pair<string,func_desc>(name,f));
}

Token *XPathEval::Evaluate(Token *token,DOMNode context)
{
	unique_ptr<TokenNodeList> lcontext(new TokenNodeList(context));
	RegisterFunction("current",{XPathFunctions::current,lcontext.get()});
	
	try
	{
		return evaluate_expr(token,xmldoc,context);
	}
	catch(Exception &e)
	{
		delete token;
		throw e;
	}
}