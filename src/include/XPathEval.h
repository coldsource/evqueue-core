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

#include <DOMNode.h>

#include <string>
#include <vector>
#include <map>

class TokenSeq;
class TokenExpr;
class Token;
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
		DOMNode current_context;
		TokenSeq *left_context;
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
	
	TokenSeq *get_child_nodes(std::string name,TokenSeq *context,TokenSeq *node_list,bool depth);
	TokenSeq *get_child_attributes(std::string name,TokenSeq *context,TokenSeq *node_list,bool depth);
	
	TokenSeq *get_all_child_nodes(std::string name,DOMNode context,TokenSeq *node_list);
	TokenSeq *get_all_child_nodes(std::string name,TokenSeq *context,TokenSeq *node_list);
	
	void filter_token_node_list(TokenSeq *list,TokenExpr *filter);
	void get_nth_token_node_list(TokenSeq *list,int n);
	
	Token *evaluate_func(const std::vector<Token *> &expr_tokens, int i,DOMNode current_context,TokenSeq *left_context);
	Token *evaluate_node(const std::vector<Token *> &expr_tokens, int i,TokenSeq *context,bool depth);
	Token *evaluate_attribute(const std::vector<Token *> &expr_tokens, int i,TokenSeq *context,bool depth);
	
	Token *evaluate_expr(Token *token,DOMNode context);
	
public:
	XPathEval(DOMDocument *xmldoc);
	
	void RegisterFunction(std::string name,func_desc f);
	
	Token *Evaluate(const std::string &xpath,DOMNode context);
};

#endif