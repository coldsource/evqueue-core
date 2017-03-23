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

#ifndef _XPATHFUNCTIONS_H_
#define _XPATHFUNCTIONS_H_

#include <XPathEval.h>

#include <vector>

class TokenNodeList;
class Token;

class XPathFunctions
{
	public:
		static Token *fntrue(XPathEval::func_context context,const std::vector<Token *> &args);
		static Token *fnfalse(XPathEval::func_context context,const std::vector<Token *> &args);
		
		static Token *name(XPathEval::func_context context,const std::vector<Token *> &args);
		
		static Token *count(XPathEval::func_context context,const std::vector<Token *> &args);
		
		static Token *substring(XPathEval::func_context context,const std::vector<Token *> &args);
		static Token *contains(XPathEval::func_context context,const std::vector<Token *> &args);
		
		static Token *current(XPathEval::func_context context,const std::vector<Token *> &args);
};

#endif