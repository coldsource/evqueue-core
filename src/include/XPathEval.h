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

#ifndef _XPATHEVAL_H_
#define _XPATHEVAL_H_

#include <string>
#include <vector>
#include <map>

class TokenNodeList;
class TokenExpr;
class Token;
class DOMNode;
class DOMDocument;

class XPathEval
{
	struct op_desc
	{
		int prio;
		Token * (*impl)(Token *lef, Token *right);
	};
	
public:
	struct func_context
	{
		TokenNodeList *context;
		void *custom_context;
		XPathEval *eval;
	};
	
	struct func_desc
	{
		Token * (*impl)(func_context context,const std::vector<Token *> &args);
		void *custom_context;
	};
	
private:
	
	std::vector<op_desc> ops_desc;
	std::map<std::string,func_desc> funcs_desc;
	
	DOMDocument *xmldoc;
	
	TokenNodeList *get_child_nodes(std::string name,DOMNode context,TokenNodeList *node_list);
	TokenNodeList *get_child_nodes(std::string name,TokenNodeList *context,TokenNodeList *node_list);
	TokenNodeList *get_child_attributes(std::string name,DOMNode context,TokenNodeList *node_list);
	TokenNodeList *get_child_attributes(std::string name,TokenNodeList *context,TokenNodeList *node_list);
	TokenNodeList *get_all_child_nodes(std::string name,DOMNode context,TokenNodeList *node_list);
	TokenNodeList *get_all_child_nodes(std::string name,TokenNodeList *context,TokenNodeList *node_list);
	
	void filter_token_node_list(TokenNodeList *list,TokenExpr *filter,DOMDocument *xmldoc,DOMNode context);
	void get_nth_token_node_list(TokenNodeList *list,int n,DOMDocument *xmldoc,DOMNode context);
	
	Token *evaluate_expr(Token *token,DOMDocument *xmldoc,DOMNode context);
	
public:
	XPathEval(DOMDocument *xmldoc);
	
	void RegisterFunction(std::string name,func_desc f);
	
	Token *Evaluate(Token *token,DOMNode context);
};

#endif