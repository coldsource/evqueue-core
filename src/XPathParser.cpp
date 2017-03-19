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

#include <XPathParser.h>
#include <XPathTokens.h>
#include <Exception.h>
#include <DOMNode.h>

using namespace std;

Token *XPathParser::parse_token(const string &s, int *pos)
{
	// Skip spaces
	while(s[*pos]==' ' && *pos<s.length())
		(*pos)++;
	
	// Match single characters
	if(s[*pos]=='(')
	{
		*pos= *pos+1;
		return new TokenSyntax(LPAR);
	}
	else if(s[*pos]==')')
	{
		*pos = *pos+1;
		return new TokenSyntax(RPAR);
	}
	else if(s[*pos]=='[')
	{
		*pos = *pos+1;
		return new TokenSyntax(LSQ);
	}
	else if(s[*pos]==']')
	{
		*pos = *pos+1;
		return new TokenSyntax(RSQ);
	}
	else if(s[*pos]==',')
	{
		*pos = *pos+1;
		return new TokenSyntax(COMMA);
	}
	
	// Match path delimiters
	if(s.substr(*pos,2)=="//")
	{
		*pos = *pos+2;
		return new TokenSyntax(DSLASH);
	}
	else if(s[*pos]=='/')
	{
		*pos = *pos+1;
		return new TokenSyntax(SLASH);
	}
	
	// Match operators
	if(s[*pos]=='+')
	{
		*pos = *pos+1;
		return new TokenOP(PLUS);
	}
	else if(s[*pos]=='-' && (*pos+1>=s.length() || !isdigit(s[*pos+1])))
	{
		*pos = *pos+1;
		return new TokenOP(MINUS);
	}
	else if(s[*pos]=='*')
	{
		*pos = *pos+1;
		return new TokenOP(MULT);
	}
	else if(s[*pos]=='=')
	{
		*pos = *pos+1;
		return new TokenOP(EQ);
	}
	else if(s.substr(*pos,2)=="!=")
	{
		*pos = *pos+2;
		return new TokenOP(NEQ);
	}
	else if(s[*pos]=='<')
	{
		*pos = *pos+1;
		return new TokenOP(LT);
	}
	else if(s.substr(*pos,2)=="<=")
	{
		*pos = *pos+2;
		return new TokenOP(LEQ);
	}
	else if(s[*pos]=='>')
	{
		*pos = *pos+1;
		return new TokenOP(GT);
	}
	else if(s.substr(*pos,2)==">=")
	{
		*pos = *pos+2;
		return new TokenOP(GEQ);
	}
	
	// Match string
	if(s[*pos]=='\'' || s[*pos]=='\"')
	{
		string buf;
		int i = *pos+1;
		
		if(s[*pos]=='\'')
			while(i<s.length() && s[i]!='\'')
				buf += s[i++];
		else if(s[*pos]=='\"')
			while(i<s.length() && s[i]!='\"')
				buf += s[i++];
		
		if(i>=s.length() || (s[i]!='\'' && s[i]!='\"'))
			throw Exception("XPath","Invalid string : end delimiter expected");
		
		*pos = i + 1;
		return new TokenString(buf);
	}
	
	// Match Integer or Float
	if(s[*pos]>='0' && s[*pos]<='9' || s[*pos]=='-')
	{
		bool is_float = false;
		
		string buf;
		if(s[*pos]=='-')
		{
			buf += '-';
			*pos++;
		}
		
		int i = *pos;
		while(i<s.length() && ((s[i]>='0' && s[i]<='9') || s[i]=='.'))
		{
			if(s[i]=='.')
			{
				if(is_float)
					throw Exception("XPath","Invalid float : only one dot is allowed");
				else
					is_float = true;
			}
			
			buf += s[i++];
		}
		
		*pos = i;
		if(is_float)
			return new TokenFloat(stod(buf));
		else
			return new TokenInt(stoi(buf));
	}
	
	// At this point we can only have Node name, Attribute or Function.
	string buf;
	int i = *pos;
	
	if(s.substr(i,2)=="..")
	{
		*pos+=2;
		return new TokenNodeName("..");
	}
	else if(s[i]=='.')
	{
		*pos+=1;
		return new TokenNodeName(".");
	}
	
	bool is_attribute = false;
	if(s.substr(i,2)=="@*")
	{
		*pos+=2;
		return new TokenAttrName("*");
	}
	else if(s[i]=='@')
	{
		is_attribute = true;
		i++;
	}
	
	while(i<s.length() && (isalnum(s[i]) || s[i]=='_' || s[i]=='-'))
		buf += s[i++];
	
	if(buf=="")
		throw Exception("XPath","Invalid node or function name");
	
	// Skip spaces
	while(s[i]==' ' && i<s.length())
		i++;
	
	*pos = i;
	if(s[i]=='(')
		return new TokenFunc(buf);
	else if(!is_attribute)
		return new TokenNodeName(buf);
	else
		return new TokenAttrName(buf);
}

vector<Token *> XPathParser::parse_expr(const string expr)
{
	vector<Token *> v;
	
	try
	{
		// Parse tokens
		int current_pos = 0;
		while(current_pos!=expr.length())
			v.push_back(parse_token(expr,&current_pos));
	}
	catch(Exception &e)
	{
		for(int i=0;i<v.size();i++)
			delete v.at(i);
		throw e;
	}
	
	return v;
}

TokenExpr *XPathParser::resolve_parenthesis(const vector<Token *> &v, int *current_pos)
{
	TokenExpr *expr_token = new TokenExpr();
	
	int i;
	for(i=*current_pos;i<v.size() && v.at(i)->GetType()!=RPAR;i++)
	{
		if(v.at(i)->GetType()==LPAR)
		{
			delete v.at(i);
			i++;
			
			expr_token->expr_tokens.push_back(resolve_parenthesis(v,&i));
			if(i>=v.size() || v.at(i)->GetType()!=RPAR)
			{
				*current_pos = i;
				delete expr_token;
				throw Exception("XPath","Missing closing parenthesis");
			}
			
			delete v.at(i);
		}
		else
			expr_token->expr_tokens.push_back(v.at(i));
	}
	
	*current_pos = i;
	return expr_token;
}

void XPathParser::prepare_functions(TokenExpr *expr)
{
	TokenFunc *func;
	
	for(int i=0;i<expr->expr_tokens.size();i++)
	{
		if(expr->expr_tokens.at(i)->GetType()==FUNC)
		{
			func = (TokenFunc *)expr->expr_tokens.at(i);
			
			if(i+1>=expr->expr_tokens.size() || expr->expr_tokens.at(i+1)->GetType()!=EXPR)
				throw Exception("XPath","Missing function parameters");
			
			TokenExpr *parameters = ((TokenExpr *)expr->expr_tokens.at(i+1));
			TokenExpr *parameter = 0;
			if(parameters->expr_tokens.size()>0)
				parameter = new TokenExpr();
			for(int j=0;j<parameters->expr_tokens.size();)
			{
				if(parameters->expr_tokens.at(j)->GetType()==COMMA)
				{
					if(parameter->expr_tokens.size()==0)
					{
						delete parameter;
						throw Exception("XPath","Empty function parameter");
					}
					
					prepare_functions(parameter);
					func->args.push_back(parameter);
					
					parameter = new TokenExpr();
					
					delete parameters->expr_tokens.at(j);
					parameters->expr_tokens.erase(parameters->expr_tokens.begin()+j);
					
					continue;
				}
				
				parameter->expr_tokens.push_back(parameters->expr_tokens.at(j));
				parameters->expr_tokens.erase(parameters->expr_tokens.begin()+j);
			}
			
			if(parameter)
			{
				if(parameter->expr_tokens.size()==0)
				{
					delete parameter;
					throw Exception("XPath","Empty function parameter");
				}
			
				prepare_functions(parameter);
				func->args.push_back(parameter);
			}
			
			// Remove expression as everything has been transfered to function arguments
			expr->expr_tokens.erase(expr->expr_tokens.begin()+i+1);
			delete parameters;
		}
		else if(expr->expr_tokens.at(i)->GetType()==EXPR)
			prepare_functions((TokenExpr *)expr->expr_tokens.at(i));
	}
}

void XPathParser::prepare_filters(TokenExpr *expr)
{
	TokenNodeName *node;
	
	for(int i=0;i<expr->expr_tokens.size();i++)
	{
		if(expr->expr_tokens.at(i)->GetType()==NODENAME)
		{
			node = (TokenNodeName *)expr->expr_tokens.at(i);
			
			if(i+1>=expr->expr_tokens.size() || expr->expr_tokens.at(i+1)->GetType()!=LSQ)
				continue;
			
			TokenExpr *filter = new TokenExpr();
			int j = i+2;
			while(j<expr->expr_tokens.size() && expr->expr_tokens.at(j)->GetType()!=RSQ)
			{
				filter->expr_tokens.push_back(expr->expr_tokens.at(j));
				j++;
			}
			
			if(j>=expr->expr_tokens.size() || expr->expr_tokens.at(j)->GetType()!=RSQ)
			{
				delete filter;
				throw Exception("XPath","Missing closing square on filter");
			}
			
			if(expr->expr_tokens.size()==0)
			{
				delete filter;
				throw Exception("XPath","Empty filter");
			}
			
			node->filter = filter;
			delete expr->expr_tokens.at(i+1);
			delete expr->expr_tokens.at(j);
			expr->expr_tokens.erase(expr->expr_tokens.begin()+i+1,expr->expr_tokens.begin()+j+1);
			
		}
		else if(expr->expr_tokens.at(i)->GetType()==FUNC)
		{
			TokenFunc *func = (TokenFunc *)expr->expr_tokens.at(i);
			for(int j=0;j<func->args.size();j++)
				prepare_filters(func->args.at(j));
		}
		else if(expr->expr_tokens.at(i)->GetType()==EXPR)
			prepare_filters((TokenExpr *)expr->expr_tokens.at(i));
	}
}

void XPathParser::disambiguish_mult(TokenExpr *expr)
{
	for(int i=0;i<expr->expr_tokens.size();i++)
	{
		if(expr->expr_tokens.at(i)->GetType()==OP && ((TokenOP *)expr->expr_tokens.at(i))->op==MULT)
		{
			if(i==0 || expr->expr_tokens.at(i-1)->GetType()==SLASH || expr->expr_tokens.at(i-1)->GetType()==DSLASH || expr->expr_tokens.at(i-1)->GetType()==OP)
			{
				delete expr->expr_tokens.at(i);
				expr->expr_tokens.erase(expr->expr_tokens.begin()+i);
				expr->expr_tokens.insert(expr->expr_tokens.begin()+i,new TokenNodeName("*"));
			}
		}
		else if(expr->expr_tokens.at(i)->GetType()==EXPR)
			disambiguish_mult((TokenExpr *)expr->expr_tokens.at(i));
	}
}

// Disambiguish operators that have textual names
void XPathParser::disambiguish_operators(TokenExpr *expr)
{
	for(int i=0;i<expr->expr_tokens.size();i++)
	{
		// Operators can be mistaken for Node names of Function names
		if(expr->expr_tokens.at(i)->GetType()==NODENAME || expr->expr_tokens.at(i)->GetType()==FUNC)
		{
			string opname;
			if(expr->expr_tokens.at(i)->GetType()==NODENAME)
				opname = ((TokenNodeName *)expr->expr_tokens.at(i))->name;
			else if(expr->expr_tokens.at(i)->GetType()==FUNC)
				opname = ((TokenFunc *)expr->expr_tokens.at(i))->name;
			
			if(i>0 && (opname=="and" || opname=="or" || opname=="div" || opname=="mod"))
			{
				Token *prec = expr->expr_tokens.at(i-1);
				if(
					prec->GetType()==LIT_STR
					|| prec->GetType()==LIT_INT
					|| prec->GetType()==LIT_FLOAT
					|| prec->GetType()==LIT_BOOL
					|| prec->GetType()==NODENAME
					|| prec->GetType()==ATTRNAME
					|| prec->GetType()==FUNC
					|| prec->GetType()==RPAR
					|| prec->GetType()==RSQ
					|| prec->GetType()==EXPR
				)
				{
					delete expr->expr_tokens.at(i);
					expr->expr_tokens.erase(expr->expr_tokens.begin()+i);
					if(opname=="and")
						expr->expr_tokens.insert(expr->expr_tokens.begin()+i,new TokenOP(AND));
					else if(opname=="or")
						expr->expr_tokens.insert(expr->expr_tokens.begin()+i,new TokenOP(OR));
					else if(opname=="div")
						expr->expr_tokens.insert(expr->expr_tokens.begin()+i,new TokenOP(DIV));
					else if(opname=="mod")
						expr->expr_tokens.insert(expr->expr_tokens.begin()+i,new TokenOP(MOD));
				}
			}
		}
		else if(expr->expr_tokens.at(i)->GetType()==EXPR)
			disambiguish_operators((TokenExpr *)expr->expr_tokens.at(i));
	}
}

TokenExpr *XPathParser::Parse(std::string xpath_expression)
{
	vector<Token *> v;
	TokenExpr *parsed_expr = 0;
	int current_pos = 0;
	
	try
	{
		v = parse_expr(xpath_expression);
		
		parsed_expr = resolve_parenthesis(v,&current_pos);
		
		if(current_pos!=v.size())
			throw Exception("XPath","Unexpected closing parenthesis");
		
		disambiguish_mult(parsed_expr);
		disambiguish_operators(parsed_expr);
		prepare_functions(parsed_expr);
		prepare_filters(parsed_expr);
	}
	catch(Exception &e)
	{
		if(parsed_expr)
			delete parsed_expr;
		for(int i=current_pos;i<v.size();i++)
			delete v.at(i);
		throw e;
	}
	
	return parsed_expr;
}