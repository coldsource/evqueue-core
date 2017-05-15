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

#ifndef _XPATHOPERATORS_H_
#define _XPATHOPERATORS_H_

#include <XPathTokens.h>

#include  <string>

class Token;

class XPathOperators
{
	static Token *operator_calc(OPERATOR op,Token *left, Token *right);
	
	public:
		static Token *Operator_PLUS(Token *left, Token *right);
		static Token *Operator_MINUS(Token *left, Token *right);
		static Token *Operator_MULT(Token *left, Token *right);
		static Token *Operator_DIV(Token *left, Token *right);
		static Token *Operator_MOD(Token *left, Token *right);
		static Token *Operator_EQ(Token *left, Token *right);
		static Token *Operator_NEQ(Token *left, Token *right);
		static Token *Operator_LT(Token *left, Token *right);
		static Token *Operator_LEQ(Token *left, Token *right);
		static Token *Operator_GT(Token *left, Token *right);
		static Token *Operator_GEQ(Token *left, Token *right);
		static Token *Operator_AND(Token *left, Token *right);
		static Token *Operator_OR(Token *left, Token *right);
		static Token *Operator_PIPE(Token *left, Token *right);
};

#endif