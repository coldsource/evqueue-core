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

#ifndef _XPATHPARSER_H_
#define _XPATHPARSER_H_

#include <vector>
#include <string>

class Token;
class TokenExpr;

class XPathParser
{
	Token *parse_token(const std::string &s, int *pos);
	std::vector<Token *> parse_expr(const std::string expr);
	TokenExpr *resolve_parenthesis(const std::vector<Token *> &v, int *current_pos);
	void prepare_functions(TokenExpr *expr);
	void prepare_filters(TokenExpr *expr);
	void disambiguish_mult(TokenExpr *expr);
	void disambiguish_operators(TokenExpr *expr);
	
	public:
		TokenExpr *Parse(std::string xpath_expression);
};

#endif