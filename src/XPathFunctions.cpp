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

#include <XPathFunctions.h>
#include <XPathTokens.h>
#include <DOMNode.h>
#include <Exception.h>

using namespace std;

Token *XPathFunctions::count(XPathEval::func_context context, const vector<Token *> &args)
{
	if(args.size()!=1)
		throw Exception("count()","Expecting 1 parameter");
	
	if(args.at(0)->GetType()!=NODELIST)
		return 0;
	
	return new TokenInt(((TokenNodeList *)args.at(0))->nodes.size());
}

Token *XPathFunctions::substring(XPathEval::func_context context, const vector<Token *> &args)
{
	if(args.size()!=2 && args.size()!=3)
		throw Exception("substring()","Expecting 2 or 3 parameters");
	
	string s = (string)(*args.at(0));
	
	if(args.size()==2)
		return new TokenString(s.substr((int)(*args.at(1))));
	else
		return new TokenString(s.substr((int)(*args.at(1)),(int)(*args.at(2))));
}

Token *XPathFunctions::current(XPathEval::func_context context, const vector<Token *> &args)
{
	if(args.size()!=0)
		throw Exception("current()","Expecting no parameters");
	
	return new TokenNodeList(*((TokenNodeList *)context.custom_context));
}