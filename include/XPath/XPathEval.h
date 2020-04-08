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

#include <DOM/DOMNode.h>

#include <string>
#include <vector>
#include <map>

#include <XPath/XPathTokens.h>

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
	struct eval_context
	{
		TokenSeq *seq;
		int index = 0;
		int size = 0;
		
		eval_context(TokenSeq *seq) { this->seq = seq; }
		eval_context(TokenSeq *seq, int index, int size) { this->seq = seq; this->index = index; this->size = size; }
		
		operator const TokenSeq*() const { return seq; }
		const TokenSeq *operator ->() const { return seq; }
	};
	
	struct func_context
	{
		eval_context current_context;
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
	
	TokenSeq *get_child_nodes(const std::string &name,const eval_context &context,TokenSeq *node_list,bool depth);
	TokenSeq *get_child_attributes(const std::string &name,const eval_context &context,TokenSeq *node_list,bool depth);
	TokenSeq *get_axis(const std::string &axis_name,const std::string &node_name,const eval_context &context,TokenSeq *node_list,bool depth);
	
	void filter_token_node_list(TokenSeq *list,TokenExpr *filter);
	void get_nth_token_node_list(TokenSeq *list,int n);
	
	Token *evaluate_func(const std::vector<Token *> &expr_tokens, int i,const eval_context &current_context,TokenSeq *left_context);
	Token *evaluate_node(const std::vector<Token *> &expr_tokens, int i,const eval_context &context,bool depth);
	Token *evaluate_axis(const std::vector<Token *> &expr_tokens, int i,const eval_context &context,bool depth);
	Token *evaluate_attribute(const std::vector<Token *> &expr_tokens, int i,const eval_context &context,bool depth);
	
	Token *evaluate_expr(Token *token,const eval_context &context);
	
public:
	XPathEval(DOMDocument *xmldoc);
	
	DOMDocument *GetXMLDoc() { return xmldoc; }
	
	void RegisterFunction(std::string name,func_desc f);
	
	Token *Evaluate(const std::string &xpath,DOMNode context);
	void Parse(const std::string &xpath);
};

#endif
