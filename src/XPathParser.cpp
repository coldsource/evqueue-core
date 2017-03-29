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

// Parse one token from the XPath expression
// It might not be possible to exactly define token type now, disambiguish will do the rest
Token *XPathParser::parse_token(const string &s, int *pos)
{
	// Skip spaces
	while(s[*pos]==' ' && *pos<s.length())
		(*pos)++;
	
	int base_pos = *pos + 1; // For human position use 1 as base
	
	if(*pos>=s.length())
		return 0;
	
	// Match single characters syntax
	if(s[*pos]=='(')
	{
		*pos= *pos+1;
		return (new TokenSyntax(LPAR))->SetInitialPosition(base_pos);
	}
	else if(s[*pos]==')')
	{
		*pos = *pos+1;
		return (new TokenSyntax(RPAR))->SetInitialPosition(base_pos);
	}
	else if(s[*pos]=='[')
	{
		*pos = *pos+1;
		return (new TokenSyntax(LSQ))->SetInitialPosition(base_pos);
	}
	else if(s[*pos]==']')
	{
		*pos = *pos+1;
		return (new TokenSyntax(RSQ))->SetInitialPosition(base_pos);
	}
	else if(s[*pos]==',')
	{
		*pos = *pos+1;
		return (new TokenSyntax(COMMA))->SetInitialPosition(base_pos);
	}
	
	// Match path delimiters
	if(s.substr(*pos,2)=="//")
	{
		*pos = *pos+2;
		return (new TokenSyntax(DSLASH))->SetInitialPosition(base_pos);
	}
	else if(s[*pos]=='/')
	{
		*pos = *pos+1;
		return (new TokenSyntax(SLASH))->SetInitialPosition(base_pos);
	}
	
	// Match operators
	if(s[*pos]=='+')
	{
		*pos = *pos+1;
		return (new TokenOP(PLUS))->SetInitialPosition(base_pos);
	}
	else if(s[*pos]=='-' && (*pos+1>=s.length() || !isdigit(s[*pos+1]))) // Not not confuse - 1 (OP + LIT_INT) and -1 (LIT_INT)
	{
		*pos = *pos+1;
		return (new TokenOP(MINUS))->SetInitialPosition(base_pos);
	}
	else if(s[*pos]=='*')
	{
		*pos = *pos+1;
		return (new TokenOP(MULT))->SetInitialPosition(base_pos); // Could also be a wildcard, see disambiguish_mult()
	}
	else if(s[*pos]=='=')
	{
		*pos = *pos+1;
		return (new TokenOP(EQ))->SetInitialPosition(base_pos);
	}
	else if(s.substr(*pos,2)=="!=")
	{
		*pos = *pos+2;
		return (new TokenOP(NEQ))->SetInitialPosition(base_pos);
	}
	else if(s[*pos]=='<')
	{
		*pos = *pos+1;
		return (new TokenOP(LT))->SetInitialPosition(base_pos);
	}
	else if(s.substr(*pos,2)=="<=")
	{
		*pos = *pos+2;
		return (new TokenOP(LEQ))->SetInitialPosition(base_pos);
	}
	else if(s[*pos]=='>')
	{
		*pos = *pos+1;
		return (new TokenOP(GT))->SetInitialPosition(base_pos);
	}
	else if(s.substr(*pos,2)==">=")
	{
		*pos = *pos+2;
		return (new TokenOP(GEQ))->SetInitialPosition(base_pos);
	}
	
	// Match strings
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
			throw Exception("XPath Parser","Invalid string : end delimiter expected for token at character "+to_string(base_pos));
		
		*pos = i + 1;
		return (new TokenString(buf))->SetInitialPosition(base_pos);
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
					throw Exception("XPath Parser","Invalid float : only one dot is allowed for token at character "+to_string(base_pos));
				else
					is_float = true;
			}
			
			buf += s[i++];
		}
		
		*pos = i;
		if(is_float)
			return (new TokenFloat(stod(buf)))->SetInitialPosition(base_pos);
		else
			return (new TokenInt(stoi(buf)))->SetInitialPosition(base_pos);
	}
	
	// At this point we can only have Node name, Attribute or Function.
	// Text operators can be confused with node or function names, see disambiguish_operators()
	string buf;
	string buf2;
	int i = *pos;
	
	if(s.substr(i,2)=="..")
	{
		*pos+=2;
		return (new TokenNodeName(".."))->SetInitialPosition(base_pos);
	}
	else if(s[i]=='.')
	{
		*pos+=1;
		return (new TokenNodeName("."))->SetInitialPosition(base_pos);
	}
	
	bool is_attribute = false;
	bool is_axis = false;
	if(s.substr(i,2)=="@*")
	{
		*pos+=2;
		return (new TokenAttrName("*"))->SetInitialPosition(base_pos);
	}
	else if(s[i]=='@')
	{
		is_attribute = true;
		i++;
	}
	
	while(i<s.length() && (isalnum(s[i]) || s[i]=='_' || s[i]=='-' || s[i]==':'))
	{
		if(s[i]==':')
		{
			if(is_axis)
				throw Exception("XPath Parser","Unexpected ':' in axis name at character "+to_string(base_pos));
			
			is_axis = true;
			if(s.substr(i,3)=="::*")
			{
				buf2 = "*";
				i+=3;
				break;
			}
			else if(s.substr(i,2)=="::")
			{
				i+=2;
				continue;
			}
		}
		
		if(!is_axis)
			buf += s[i++];
		else
			buf2 += s[i++];
	}
	
	if(buf=="")
		throw Exception("XPath Parser","Invalid node or function name at character "+to_string(base_pos));
	
	if(is_axis && buf2=="")
		throw Exception("XPath Parser","Invalid axis filter node at character "+to_string(base_pos));
	
	// Skip spaces
	while(s[i]==' ' && i<s.length())
		i++;
	
	// Function names only differ from node names because they have parenthesis
	*pos = i;
	if(is_attribute)
		return (new TokenAttrName(buf))->SetInitialPosition(base_pos);
	else if(is_axis)
		return (new TokenAxis(buf,buf2))->SetInitialPosition(base_pos);
	else if(s[i]=='(')
		return (new TokenFunc(buf))->SetInitialPosition(base_pos);
	else
		return (new TokenNodeName(buf))->SetInitialPosition(base_pos);
}

// Create a TokenExpr from a string by calling parse_token
vector<Token *> XPathParser::parse_expr(const string expr)
{
	vector<Token *> v;
	
	try
	{
		// Parse tokens
		int current_pos = 0;
		Token *token;
		while(current_pos!=expr.length() && (token = parse_token(expr,&current_pos)))
			v.push_back(token);
	}
	catch(Exception &e)
	{
		// Free already created tokens
		for(int i=0;i<v.size();i++)
			delete v.at(i);
		throw e;
	}
	
	return v;
}

// Create sub expressions with each group of parenthesis
// Functions arguments are treated as expressions
TokenExpr *XPathParser::resolve_parenthesis(const vector<Token *> &v, int *current_pos)
{
	TokenExpr *expr_token = new TokenExpr();
	
	int i;
	// Loop until end of document or end of expression (ie right parenthesis)
	for(i=*current_pos;i<v.size() && v.at(i)->GetType()!=RPAR;i++)
	{
		// Left parenthesis, we have a sub expression
		if(v.at(i)->GetType()==LPAR)
		{
			delete v.at(i);
			i++;
			
			// Recursively build sub expression
			expr_token->expr_tokens.push_back(resolve_parenthesis(v,&i));
			if(i>=v.size() || v.at(i)->GetType()!=RPAR)
			{
				*current_pos = i;
				delete expr_token;
				throw Exception("XPath Parser","Missing closing parenthesis");
			}
			
			delete v.at(i);
		}
		else
			expr_token->expr_tokens.push_back(v.at(i)); // Simply push token to current expression
	}
	
	*current_pos = i;
	return expr_token;
}

// Look for expressions after function names and parse arguments
// Function arguments are treated as arrays of expressions, one expression for each argument
void XPathParser::prepare_functions(TokenExpr *expr)
{
	TokenFunc *func;
	
	for(int i=0;i<expr->expr_tokens.size();i++)
	{
		// Loof for a function name
		if(expr->expr_tokens.at(i)->GetType()==FUNC)
		{
			func = (TokenFunc *)expr->expr_tokens.at(i);
			
			// Function names must by followed by an expression (ie the parameters)
			if(i+1>=expr->expr_tokens.size() || expr->expr_tokens.at(i+1)->GetType()!=EXPR)
				throw Exception("XPath Parser","Missing function parameters"+func->LogInitialPosition());
			
			TokenExpr *parameters = ((TokenExpr *)expr->expr_tokens.at(i+1));
			TokenExpr *parameter = 0;
			
			if(parameters->expr_tokens.size()>0)
				parameter = new TokenExpr();
			
			// Split parameters expression on comma to separate parameters
			for(int j=0;j<parameters->expr_tokens.size();)
			{
				if(parameters->expr_tokens.at(j)->GetType()==COMMA)
				{
					// Store last found parameters and prepare for new one
					if(parameter->expr_tokens.size()==0)
					{
						delete parameter;
						throw Exception("XPath Parser","Empty function parameter"+func->LogInitialPosition());
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
			
			// Store last found parameter
			if(parameter)
			{
				if(parameter->expr_tokens.size()==0)
				{
					delete parameter;
					throw Exception("XPath Parser","Empty function parameter"+func->LogInitialPosition());
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

// Create expressions from filters (expressions between square brackets)
// Filters are removed from XPath expression and directly attached to node names
void XPathParser::prepare_filters(TokenExpr *expr)
{
	TokenNodeName *node;
	
	for(int i=0;i<expr->expr_tokens.size();i++)
	{
		// Look for node names as ony them can have filters
		if(expr->expr_tokens.at(i)->GetType()==LSQ)
		{
			// Catch filter expression until right bracket ('filter end')
			TokenExpr *filter = new TokenExpr();
			int j = i+1;
			while(j<expr->expr_tokens.size() && expr->expr_tokens.at(j)->GetType()!=RSQ)
			{
				filter->expr_tokens.push_back(expr->expr_tokens.at(j));
				j++;
			}
			
			if(j>=expr->expr_tokens.size() || expr->expr_tokens.at(j)->GetType()!=RSQ)
			{
				delete filter;
				throw Exception("XPath Parser","Missing closing square on filter");
			}
			
			if(expr->expr_tokens.size()==0)
			{
				delete filter;
				throw Exception("XPath Parser","Empty filter");
			}
			
			delete expr->expr_tokens.at(i);
			delete expr->expr_tokens.at(j);
			expr->expr_tokens.erase(expr->expr_tokens.begin()+i,expr->expr_tokens.begin()+j+1);
			expr->expr_tokens.insert(expr->expr_tokens.begin()+i,new TokenFilter(filter));
			
		}
		else if(expr->expr_tokens.at(i)->GetType()==FUNC)
		{
			// Parse function parameters as they contains expressions
			TokenFunc *func = (TokenFunc *)expr->expr_tokens.at(i);
			for(int j=0;j<func->args.size();j++)
				prepare_filters(func->args.at(j));
		}
		else if(expr->expr_tokens.at(i)->GetType()==EXPR) // Parse sub expresionns
			prepare_filters((TokenExpr *)expr->expr_tokens.at(i));
	}
}

// Mult operator (1 * 1) can be confused with node wildcard (/node/*/node)
// On parsing '*' is always resolved as operator, transform to node name when needed
void XPathParser::disambiguish_mult(TokenExpr *expr)
{
	for(int i=0;i<expr->expr_tokens.size();i++)
	{
		// Look for operator MULT
		if(expr->expr_tokens.at(i)->GetType()==OP && ((TokenOP *)expr->expr_tokens.at(i))->op==MULT)
		{
			// When found as first token, we have a node name
			// When preceding token is /, // or an operator we have a node name
			// Case 1 : '*' (expression begin)
			// Case 2 : 'node/* (after /)
			// Case 3 : //* (after //)
			// Case 4 : 2 * * (after an operator)
			if(i==0 || expr->expr_tokens.at(i-1)->GetType()==SLASH || expr->expr_tokens.at(i-1)->GetType()==DSLASH || expr->expr_tokens.at(i-1)->GetType()==OP)
			{
				int pos = expr->expr_tokens.at(i)->GetInitialPosition();
				delete expr->expr_tokens.at(i);
				expr->expr_tokens.erase(expr->expr_tokens.begin()+i);
				expr->expr_tokens.insert(expr->expr_tokens.begin()+i,(new TokenNodeName("*"))->SetInitialPosition(pos));
			}
		}
		else if(expr->expr_tokens.at(i)->GetType()==EXPR)
			disambiguish_mult((TokenExpr *)expr->expr_tokens.at(i)); // Recursively parse sub expressions
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
			// Get potential operator name
			string opname;
			if(expr->expr_tokens.at(i)->GetType()==NODENAME)
				opname = ((TokenNodeName *)expr->expr_tokens.at(i))->name;
			else if(expr->expr_tokens.at(i)->GetType()==FUNC)
				opname = ((TokenFunc *)expr->expr_tokens.at(i))->name;
			
			// This an operator if :
			// 1. Name matches a potential operator
			// 2. It is preceded by the below tokens
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
					// Replace token with operator
					int pos = expr->expr_tokens.at(i)->GetInitialPosition();
					delete expr->expr_tokens.at(i);
					expr->expr_tokens.erase(expr->expr_tokens.begin()+i);
					if(opname=="and")
						expr->expr_tokens.insert(expr->expr_tokens.begin()+i,(new TokenOP(AND))->SetInitialPosition(pos));
					else if(opname=="or")
						expr->expr_tokens.insert(expr->expr_tokens.begin()+i,(new TokenOP(OR))->SetInitialPosition(pos));
					else if(opname=="div")
						expr->expr_tokens.insert(expr->expr_tokens.begin()+i,(new TokenOP(DIV))->SetInitialPosition(pos));
					else if(opname=="mod")
						expr->expr_tokens.insert(expr->expr_tokens.begin()+i,(new TokenOP(MOD))->SetInitialPosition(pos));
				}
			}
		}
		else if(expr->expr_tokens.at(i)->GetType()==EXPR)
			disambiguish_operators((TokenExpr *)expr->expr_tokens.at(i));
	}
}

// Parse the whole XPath expression and return the corresponding expression
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
			throw Exception("XPath Parser","Unexpected closing parenthesis");
		
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