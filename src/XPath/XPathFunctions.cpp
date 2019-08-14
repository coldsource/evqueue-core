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

#include <XPath/XPathFunctions.h>
#include <XPath/XPathTokens.h>
#include <DOM/DOMNode.h>
#include <Exception/Exception.h>

using namespace std;

Token *XPathFunctions::fntrue(XPathEval::func_context context,const std::vector<Token *> &args)
{
	return new TokenBool(true);
}

Token *XPathFunctions::fnfalse(XPathEval::func_context context,const std::vector<Token *> &args)
{
	return new TokenBool(false);
}

Token *XPathFunctions::fnnot(XPathEval::func_context context,const std::vector<Token *> &args)
{
	if(args.size()!=1)
		throw Exception("not()","Expecting 1 parameter");
	
	return new TokenBool(!((bool)(*args.at(0))));
}

Token *XPathFunctions::name(XPathEval::func_context context,const std::vector<Token *> &args)
{
	if(args.size()>1)
		throw Exception("name()","Expecting 0 or 1 parameters");
	
	const TokenSeq *seq;
	if(args.size()==1)
	{
		if(args.at(0)->GetType()!=SEQ)
			throw Exception("name()","Expecting sequence as parameter");
		
		seq = (TokenSeq *)args.at(0);
	}
	else
	{
		if(context.left_context->items.size()>0)
			seq = context.left_context;
		else
			seq = context.current_context;
	}
	
	TokenSeq *result_seq = new TokenSeq();
	for(int i=0;i<seq->items.size();i++)
		result_seq->items.push_back(new TokenString(((DOMNode)*seq->items.at(i)).getNodeName()));
	
	return result_seq;
}

Token *XPathFunctions::count(XPathEval::func_context context, const vector<Token *> &args)
{
	if(args.size()!=1)
		throw Exception("count()","Expecting 1 parameter");
	
	if(args.at(0)->GetType()!=SEQ)
		throw Exception("count()","Expecting sequence as parameter");
	
	return new TokenInt(((TokenSeq *)args.at(0))->items.size());
}


Token *XPathFunctions::min(XPathEval::func_context context, const vector<Token *> &args)
{
	if(args.size()!=1)
		throw Exception("max()","Expecting 1 parameter");
	
	if(args.at(0)->GetType()!=SEQ)
		throw Exception("count()","Expecting sequence as parameter");
	
	TokenSeq *seq = (TokenSeq *)args.at(0);
	if(seq->items.size()==0)
		return new TokenSeq();
	
	int min = (double)(*seq->items.at(0));
	for(int i=1;i<seq->items.size();i++)
		if((double)(*seq->items.at(i))<min)
			min = (double)(*seq->items.at(i));
	
	return new TokenFloat(min);
}

Token *XPathFunctions::max(XPathEval::func_context context, const vector<Token *> &args)
{
	if(args.size()!=1)
		throw Exception("max()","Expecting 1 parameter");
	
	if(args.at(0)->GetType()!=SEQ)
		throw Exception("count()","Expecting sequence as parameter");
	
	TokenSeq *seq = (TokenSeq *)args.at(0);
	if(seq->items.size()==0)
		return new TokenSeq();
	
	int max = (double)(*seq->items.at(0));
	for(int i=1;i<seq->items.size();i++)
		if((double)(*seq->items.at(i))>max)
			max = (double)(*seq->items.at(i));
	
	return new TokenFloat(max);
}

Token *XPathFunctions::position(XPathEval::func_context context, const vector<Token *> &args)
{
	if(context.left_context->items.size()>0)
	{
		TokenSeq *seq = new TokenSeq();
		for(int i=0;i<context.left_context->items.size();i++)
			seq->items.push_back(new TokenInt(i+1));
		return seq;
	}
	
	return new TokenInt(context.current_context.index);
}

Token *XPathFunctions::last(XPathEval::func_context context, const vector<Token *> &args)
{
	if(context.left_context->items.size()>0)
	{
		TokenSeq *seq = new TokenSeq();
		for(int i=0;i<context.left_context->items.size();i++)
			seq->items.push_back(new TokenInt(context.left_context->items.size()));
		return seq;
	}
	
	return new TokenInt(context.current_context.size);
}

Token *XPathFunctions::string_length(XPathEval::func_context context, const vector<Token *> &args)
{
	if(args.size()!=1)
		throw Exception("substring()","Expecting 1 parameter");
	
	string s = (string)(*args.at(0));
	return new TokenInt(s.length());
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

Token *XPathFunctions::contains(XPathEval::func_context context, const vector<Token *> &args)
{
	if(args.size()!=2)
		throw Exception("contains()","Expecting 2 parameters");
	
	string s1 = (string)(*args.at(0));
	string s2 = (string)(*args.at(1));
	
	return new TokenBool(s1.find(s2)!=string::npos);
}

Token *XPathFunctions::current(XPathEval::func_context context, const vector<Token *> &args)
{
	if(args.size()!=0)
		throw Exception("current()","Expecting no parameters");
	
	return new TokenSeq(*((TokenSeq *)context.custom_context));
}