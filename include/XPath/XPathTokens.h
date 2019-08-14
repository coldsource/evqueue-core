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

#ifndef _XPATHTOKENS_H_
#define _XPATHTOKENS_H_

#include <DOM/DOMNode.h>

#include <string>
#include <vector>

// Token types that can be found in XPath expression
enum TOKEN_TYPE
{
	LIT_STR, // Literal string
	LIT_INT, // Literal integer
	LIT_FLOAT, // Literal float or double
	LIT_BOOL, // Literal boolean
	NODENAME, // Node name to match with DOM
	ATTRNAME, // Attribute name to match with DOM
	AXIS, // Axis to match with DOM
	FUNC, // Function
	FILTER, // Filter
	OP, // Operator
	LPAR, // Left parenthesis
	RPAR, // Right parenthesis
	LSQ, // Left square bracket
	RSQ, // Right square bracket
	COMMA, // Comma
	SLASH, // Slash path separator
	DSLASH, // Double slash path separator
	EXPR, // Expression, a complex XPath expression, full or partial
	NODE, // Resolved node hat points to dom elements
	SEQ, // XPath sequence
	ENDLINE // Special token to identify line end
};

// Operators types
enum OPERATOR
{
	MULT, // *
	DIV, // div
	MOD, // mod
	PLUS, // +
	MINUS, // -
	LT, // <
	LEQ, // <=
	GT, // >
	GEQ, // >=
	EQ, // =
	NEQ, // !=
	AND, // and
	OR, // or
	PIPE // |
};

// Definition of tokens
class Token
{
	int cast_string_to_int(const std::string &s) const;
	double cast_string_to_double(const std::string &s) const;
	
	int initial_position = -1;
	
public:
	Token();
	Token(const Token &t);
	virtual ~Token() {};
	
	Token *SetInitialPosition(int pos);
	int GetInitialPosition() { return initial_position; }
	std::string LogInitialPosition() const;
	
	virtual TOKEN_TYPE GetType() const = 0;
	virtual Token *clone() = 0;
	
	virtual operator bool() const = 0;
	
	operator int() const;
	operator double() const;
	operator std::string() const;
	operator DOMNode() const;
	
	static std::string ToString(TOKEN_TYPE type);
	static std::string ToString(OPERATOR op);
};

class TokenSyntax:public Token
{
public:
	TOKEN_TYPE type;
	
	TokenSyntax(TOKEN_TYPE type) { this->type = type; }
	TokenSyntax(const TokenSyntax &t):Token(t) { type = t.type; }
	
	TOKEN_TYPE GetType() const { return type; }
	Token *clone() { return new TokenSyntax(*this); }
	
	operator bool() const { return false; }
};

class TokenOP:public Token
{
public:
	OPERATOR op;
	
	TokenOP(OPERATOR op) { this->op = op; }
	TokenOP(const TokenOP &o):Token(o) { op=o.op; }
	
	TOKEN_TYPE GetType() const { return OP; }
	Token *clone() { return new TokenOP(*this); }
	
	operator bool() const { return false; }
};

class TokenString:public Token
{
public:
	std::string s;
	
	TokenString(std::string s) { this->s = s; }
	TokenString(const TokenString &ts):Token(ts) { s = ts.s; }
	
	TOKEN_TYPE GetType() const { return LIT_STR; }
	Token *clone() { return new TokenString(*this); }
	
	operator bool() const { return s.length(); }
};

class TokenFloat:public Token
{
public:
	double d;
	
	TokenFloat(double d) { this->d = d; }
	TokenFloat(const TokenFloat &tf):Token(tf) { d = tf.d; }
	
	TOKEN_TYPE GetType() const { return LIT_FLOAT; }
	Token *clone() { return new TokenFloat(*this); }
	
	operator bool() const { return d; }
};

class TokenInt:public Token
{
public:
	int i;
	
	TokenInt(int i) { this->i = i; }
	TokenInt(const TokenInt &tf):Token(tf) { i = tf.i; }
	
	TOKEN_TYPE GetType() const { return LIT_INT; }
	Token *clone() { return new TokenInt(*this); }
	
	operator bool() const { return i; }
};

class TokenBool:public Token
{
public:
	bool b;
	
	TokenBool(bool b) { this->b = b; }
	TokenBool(const TokenBool &tb):Token(tb) { b = tb.b; }
	
	TOKEN_TYPE GetType() const { return LIT_BOOL; }
	Token *clone() { return new TokenBool(*this); }
	
	operator bool() const { return b; }
};

class TokenExpr:public Token
{
public:
	std::vector<Token *> expr_tokens;
	
	TokenExpr() { }
	TokenExpr(const TokenExpr &expr);
	~TokenExpr();
	
	TOKEN_TYPE GetType() const { return EXPR; }
	Token *clone() { return new TokenExpr(*this); }
	
	operator bool() const { return false; }
};

class TokenNodeName:public Token
{
public:
	std::string name;
	
	TokenNodeName(const std::string &name) { this->name = name; }
	TokenNodeName(const TokenNodeName &node_name);
	
	TOKEN_TYPE GetType() const { return NODENAME; }
	Token *clone() { return new TokenNodeName(*this); }
	
	operator bool() const { return false; }
};

class TokenAttrName:public Token
{
public:
	std::string name;
	
	TokenAttrName(const std::string &name) { this->name = name; }
	TokenAttrName(const TokenAttrName &attr_name);
	
	TOKEN_TYPE GetType() const { return ATTRNAME; }
	Token *clone() { return new TokenAttrName(*this); }
	
	operator bool() const { return false; }
};

class TokenAxis:public Token
{
public:
	std::string name;
	std::string node_name;
	
	TokenAxis(const std::string &name,const std::string &node_name) { this->name = name; this->node_name = node_name; }
	TokenAxis(const TokenAxis &axis);
	
	TOKEN_TYPE GetType() const { return AXIS; }
	Token *clone() { return new TokenAxis(*this); }
	
	operator bool() const { return false; }
};

class TokenFilter:public Token
{
public:
	TokenExpr *filter = 0;
	
	TokenFilter(TokenExpr *filter) { this->filter = filter; }
	TokenFilter(const TokenFilter &tf);
	~TokenFilter();
	
	TOKEN_TYPE GetType() const { return FILTER; }
	Token *clone() { return new TokenFilter(*this); }
	
	operator bool() const { return false; }
};

class TokenFunc:public Token
{
public:
	std::string name;
	std::vector<TokenExpr *> args;
	
	TokenFunc(const std::string &name) { this->name = name; }
	TokenFunc(const TokenFunc &f);
	~TokenFunc();
	
	TOKEN_TYPE GetType() const { return FUNC; }
	Token *clone() { return new TokenFunc(*this); }
	
	operator bool() const { return false; }
};

class TokenNode:public Token
{
public:
	DOMNode node;
	
	TokenNode(const DOMNode &node) { this->node = node; }
	TokenNode(const TokenNode &tn):Token(tn) { node = tn.node; }
	
	TOKEN_TYPE GetType() const { return NODE; }
	Token *clone() { return new TokenNode(*this); }
	
	operator bool() const { return node; }
};

class TokenSeq:public Token
{
public:
	std::vector<Token *> items;
	
	TokenSeq() { }
	TokenSeq(Token *token);
	TokenSeq(const TokenSeq &list);
	~TokenSeq();
	
	TOKEN_TYPE GetType() const { return SEQ; }
	Token *clone() { return new TokenSeq(*this); }
	
	operator bool() const { return items.size(); }
};

#endif