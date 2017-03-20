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

#include <string>
#include <vector>

class DOMNode;

// Token types that can be found in XPath expression
enum TOKEN_TYPE
{
	LIT_STR, // Literal string
	LIT_INT, // Literal integer
	LIT_FLOAT, // Literal float or double
	LIT_BOOL, // Literal boolean
	NODENAME, // Node name to match with DOM
	ATTRNAME, // Attribute name to match with DOM
	FUNC, // Function
	OP, // Operator
	LPAR, // Left parenthesis
	RPAR, // Right parenthesis
	LSQ, // Left square bracket
	RSQ, // Right square bracket
	COMMA, // Comma
	SLASH, // Slash path separator
	DSLASH, // Double slash path separator
	EXPR, // Expression, a complex XPath expression, full or partial
	NODELIST // Resolved node list that points to dom elements
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
	OR// or
};

// Definition of tokens
class Token
{
	static int cast_string_to_int(const std::string &s);
	static double cast_string_to_double(const std::string &s);
	
	static int cast_token_to_int(const Token *token);
	static double cast_token_to_double(const Token *token);
	static std::string cast_token_to_string(const Token *token);
	
public:
	virtual ~Token() {};
	
	virtual TOKEN_TYPE GetType() const = 0;
	virtual Token *clone() = 0;
	
	virtual operator bool() const = 0;
	
	operator int() const;
	operator double() const;
	operator std::string() const;
};

class TokenSyntax:public Token
{
public:
	TOKEN_TYPE type;
	
	TokenSyntax(TOKEN_TYPE type) { this->type = type; }
	TokenSyntax(TokenSyntax &t) { type = t.type; }
	
	TOKEN_TYPE GetType() const { return type; }
	Token *clone() { return new TokenSyntax(*this); }
	
	operator bool() const { return false; }
};

class TokenOP:public Token
{
public:
	OPERATOR op;
	
	TokenOP(OPERATOR op) { this->op = op; }
	TokenOP(TokenOP &o) { op=o.op; }
	
	TOKEN_TYPE GetType() const { return OP; }
	Token *clone() { return new TokenOP(*this); }
	
	operator bool() const { return false; }
};

class TokenString:public Token
{
public:
	std::string s;
	
	TokenString(std::string s) { this->s = s; }
	TokenString(TokenString &ts) { s = ts.s; }
	
	TOKEN_TYPE GetType() const { return LIT_STR; }
	Token *clone() { return new TokenString(*this); }
	
	operator bool() const { return s.length(); }
};

class TokenFloat:public Token
{
public:
	double d;
	
	TokenFloat(double d) { this->d = d; }
	TokenFloat(TokenFloat &tf) { d = tf.d; }
	
	TOKEN_TYPE GetType() const { return LIT_FLOAT; }
	Token *clone() { return new TokenFloat(*this); }
	
	operator bool() const { return d; }
};

class TokenInt:public Token
{
public:
	int i;
	
	TokenInt(int i) { this->i = i; }
	TokenInt(TokenInt &tf) { i = tf.i; }
	
	TOKEN_TYPE GetType() const { return LIT_INT; }
	Token *clone() { return new TokenInt(*this); }
	
	operator bool() const { return i; }
};

class TokenBool:public Token
{
public:
	bool b;
	
	TokenBool(bool b) { this->b = b; }
	TokenBool(TokenBool &tb) { b = tb.b; }
	
	TOKEN_TYPE GetType() const { return LIT_BOOL; }
	Token *clone() { return new TokenBool(*this); }
	
	operator bool() const { return b; }
};

class TokenExpr:public Token
{
public:
	std::vector<Token *> expr_tokens;
	
	TokenExpr() { }
	TokenExpr(TokenExpr &expr);
	~TokenExpr();
	
	TOKEN_TYPE GetType() const { return EXPR; }
	Token *clone() { return new TokenExpr(*this); }
	
	operator bool() const { return false; }
};

class TokenNodeName:public Token
{
public:
	std::string name;
	TokenExpr *filter = 0;
	
	TokenNodeName(std::string name) { this->name = name; }
	TokenNodeName(TokenNodeName &node_name);
	~TokenNodeName();
	
	TOKEN_TYPE GetType() const { return NODENAME; }
	Token *clone() { return new TokenNodeName(*this); }
	
	operator bool() const { return false; }
};

class TokenAttrName:public Token
{
public:
	std::string name;
	TokenExpr *filter = 0;
	
	TokenAttrName(std::string name) { this->name = name; }
	TokenAttrName(TokenAttrName &attr_name);
	~TokenAttrName();
	
	TOKEN_TYPE GetType() const { return ATTRNAME; }
	Token *clone() { return new TokenAttrName(*this); }
	
	operator bool() const { return false; }
};

class TokenFunc:public Token
{
public:
	std::string name;
	std::vector<TokenExpr *> args;
	
	TokenFunc(std::string name) { this->name = name; }
	TokenFunc(TokenFunc &f);
	~TokenFunc();
	
	TOKEN_TYPE GetType() const { return FUNC; }
	Token *clone() { return new TokenFunc(*this); }
	
	operator bool() const { return false; }
};

class TokenNodeList:public Token
{
public:
	std::vector<DOMNode> nodes;
	
	TokenNodeList() { }
	TokenNodeList(DOMNode node);
	TokenNodeList(TokenNodeList &list);
	
	TOKEN_TYPE GetType() const { return NODELIST; }
	Token *clone() { return new TokenNodeList(*this); }
	
	operator bool() const { return nodes.size(); }
};

#endif