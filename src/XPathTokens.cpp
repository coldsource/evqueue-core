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

#include <XPathTokens.h>
#include <DOMNode.h>
#include <Exception.h>

using namespace std;

int Token::cast_string_to_int(const string &s) const
{
	try
	{
		size_t pos;
		int n = stoi(s,&pos);
		if(pos!=s.length())
			throw Exception("Type Cast","Could not cast string to int"+LogInitialPosition());
		return  n;
	}
	catch(...)
	{
		throw Exception("Type Cast","Could not cast string to int"+LogInitialPosition());
	}
}

double Token::cast_string_to_double(const string &s) const
{
	try
	{
		size_t pos;
		int d = stod(s,&pos);
		if(pos!=s.length())
			throw Exception("Type Cast","Could not cast string to float"+LogInitialPosition());
		return d;
	}
	catch(...)
	{
		throw Exception("Type Cast","Could not cast string to float"+LogInitialPosition());
	}
}

Token::Token()
{
	initial_position = -1;
}

Token::Token(const Token &t)
{
	initial_position = t.initial_position;
}

Token *Token::SetInitialPosition(int pos)
{
	initial_position = pos;
	return this;
}
string Token::LogInitialPosition() const
{
	if(initial_position==-1)
		return "";
	return " at character "+to_string(initial_position);
}

Token::operator int() const
{
	if(this->GetType()==LIT_FLOAT)
		throw Exception("XPath","Could not cast float to int"+LogInitialPosition());
	else if(this->GetType()==LIT_INT)
		return ((TokenInt *)this)->i;
	else if(this->GetType()==LIT_BOOL)
		return ((TokenBool *)this)->b?1:0;
	else if(this->GetType()==LIT_STR)
		return cast_string_to_int(((TokenString *)this)->s);
	else if(this->GetType()==NODE)
		return cast_string_to_int(((TokenNode *)this)->node.getNodeValue());
	else if(this->GetType()==SEQ && ((TokenSeq *)this)->items.size()==1)
	{
		try
		{
			return (int)*(((TokenSeq *)this)->items.at(0));
		}
		catch(Exception &e)
		{
			e.error+=LogInitialPosition();
			throw e;
		}
	}
	throw Exception("Type Cast","Incompatible type for operand"+LogInitialPosition());
}

Token::operator double() const
{
	if(this->GetType()==LIT_FLOAT)
		return ((TokenFloat *)this)->d;
	else if(this->GetType()==LIT_INT)
		return ((TokenInt *)this)->i;
	else if(this->GetType()==LIT_BOOL)
		return ((TokenBool *)this)->b?1:0;
	else if(this->GetType()==LIT_STR)
		return cast_string_to_double(((TokenString *)this)->s);
	else if(this->GetType()==NODE)
		return cast_string_to_double(((TokenNode *)this)->node.getNodeValue());
	else if(this->GetType()==SEQ && ((TokenSeq *)this)->items.size()==1)
	{
		try
		{
			(double)*(((TokenSeq *)this)->items.at(0));
		}
		catch(Exception &e)
		{
			e.error+=LogInitialPosition();
			throw e;
		}
	}
	throw Exception("Type Cast","Incompatible type for operand"+LogInitialPosition());
}

Token::operator string() const
{
	if(GetType()==LIT_STR)
		return ((TokenString *)this)->s;
	else if(GetType()==LIT_INT)
		return to_string(((TokenInt *)this)->i);
	else if(GetType()==LIT_FLOAT)
		return to_string(((TokenFloat *)this)->d);
	else if(GetType()==LIT_BOOL)
		return to_string(((TokenBool *)this)->b);
	else if(this->GetType()==NODE)
		return ((TokenNode *)this)->node.getNodeValue();
	else if(GetType()==SEQ)
	{
		if(((TokenSeq *)this)->items.size()==0)
			return "";
		else if(((TokenSeq *)this)->items.size()==1)
		{
			try
			{
				return (string)*(((TokenSeq *)this)->items.at(0));
			}
			catch(Exception &e)
			{
				e.error+=LogInitialPosition();
				throw e;
			}
		}
	}
	throw Exception("Type Cast","Incompatible type for operand"+LogInitialPosition());
}

Token::operator DOMNode() const
{
	if(GetType()==NODE)
		return ((TokenNode *)this)->node;
	else if(this->GetType()==SEQ && ((TokenSeq *)this)->items.size()==1)
		return (DOMNode)*(((TokenSeq *)this)->items.at(0));
	throw Exception("Type Cast","Incompatible type for operand"+LogInitialPosition());
}

string Token::ToString(TOKEN_TYPE type)
{
	if(type==LIT_STR) return "LIT_STR";
	if(type==LIT_INT) return "LIT_INT";
	if(type==LIT_FLOAT) return "LIT_FLOAT";
	if(type==LIT_BOOL) return "LIT_BOOL";
	if(type==NODENAME) return "NODENAME";
	if(type==ATTRNAME) return "ATTRNAME";
	if(type==FUNC) return "FUNC";
	if(type==OP) return "OP";
	if(type==LPAR) return "LPAR";
	if(type==RPAR) return "RPAR";
	if(type==LSQ) return "LSQ";
	if(type==RSQ) return "RSQ";
	if(type==COMMA) return "COMMA";
	if(type==SLASH) return "SLASH";
	if(type==DSLASH) return "DSLASH";
	if(type==EXPR) return "EXPR";
	if(type==NODE) return "NODE";
	if(type==SEQ) return "SEQ";
	return "UNKNOWN";
}

string Token::ToString(OPERATOR op)
{
	if(op==MULT) return "*";
	if(op==DIV) return "div";
	if(op==MOD) return "mod";
	if(op==PLUS) return "+";
	if(op==MINUS) return "-";
	if(op==LT) return "<";
	if(op==LEQ) return "<=";
	if(op==GT) return ">";
	if(op==GEQ) return ">=";
	if(op==EQ) return "=";
	if(op==NEQ) return "!=";
	if(op==AND) return "and";
	if(op==OR) return "or";
	return "UNKNOWN";
}

TokenExpr::TokenExpr(const TokenExpr &expr):Token(expr)
{
	for(int i=0;i<expr.expr_tokens.size();i++)
		expr_tokens.push_back(expr.expr_tokens.at(i)->clone());
}

TokenExpr::~TokenExpr()
{
	for(int i=0;i<expr_tokens.size();i++)
		delete expr_tokens.at(i);
}

TokenNodeName::TokenNodeName(const TokenNodeName &node_name):Token(node_name)
{
	name = node_name.name;
	if(filter)
		filter = new TokenExpr(*(node_name.filter));
	else
		filter = 0;
}

TokenNodeName::~TokenNodeName()
{
	if(filter)
		delete filter;
}

TokenAttrName::TokenAttrName(const TokenAttrName &attr_name):Token(attr_name)
{
	name = attr_name.name;
	if(filter)
		filter = new TokenExpr(*(attr_name.filter));
	else
		filter = 0;
}

TokenAttrName::~TokenAttrName()
{
	if(filter)
		delete filter;
}

TokenFunc::TokenFunc(const TokenFunc &f):Token(f)
{
	name = f.name;
	for(int i=0;i<args.size();i++)
		args.push_back(new TokenExpr(*args.at(i)));
}

TokenFunc::~TokenFunc()
{
	for(int i=0;i<args.size();i++)
		delete args.at(i);
}

TokenSeq::TokenSeq(Token *token)
{
	items.push_back(token);
}

TokenSeq::TokenSeq(const TokenSeq &list):Token(list)
{
	for(int i=0;i<list.items.size();i++)
		items.push_back(list.items.at(i)->clone());
}

TokenSeq::~TokenSeq()
{
	for(int i=0;i<items.size();i++)
		delete items.at(i);
}