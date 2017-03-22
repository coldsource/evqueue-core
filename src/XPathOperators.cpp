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

#include <XPathOperators.h>
#include <XPathTokens.h>
#include <DOMNode.h>
#include <Exception.h>

using namespace std;

// Used for computation operators that operates on numbers (+ - div mul mod)
Token *XPathOperators::operator_calc(OPERATOR op,Token *left, Token *right)
{
	bool type_int = false,type_double = false;
	int left_i,right_i;
	double left_d,right_d;
	
	try
	{
		// Try to cas as integers
		left_i = (int)(*left);
		right_i = (int)(*right);
		type_int = true;
	}
	catch(...)
	{
		// Casting as integers failed, try float
		if(op==MOD)
			throw Exception("Operator MOD","Modulus operator can only operate on integers");
		
		left_d = (double)(*left);
		right_d = double(*right);
		type_double = true;
	}
	
	if(type_int)
	{
		// Computation on integers
		if(op==PLUS)
			return new TokenInt(left_i+right_i);
		else if(op==MINUS)
			return new TokenInt(left_i-right_i);
		else if(op==MULT)
			return new TokenInt(left_i*right_i);
		else if(op==DIV)
			return new TokenFloat((double)left_i/(double)right_i);
		else if(op==MOD)
			return new TokenInt(left_i%right_i);
	}
	else if(type_double)
	{
		// Computation on float
		if(op==PLUS)
			return new TokenFloat(left_d+right_d);
		else if(op==MINUS)
			return new TokenFloat(left_d-right_d);
		else if(op==MULT)
			return new TokenFloat(left_d*right_d);
		else if(op==DIV)
			return new TokenFloat(left_d/right_d);
		// Modulus can only be computed on integers
	}
	
	// Could not cast operands as numbers
	throw Exception("Operator "+Token::ToString(op),"Error evaluating operator");
}

Token* XPathOperators::Operator_PLUS(Token* left, Token* right)
{
	return operator_calc(PLUS,left,right);
}

Token* XPathOperators::Operator_MINUS(Token* left, Token* right)
{
	return operator_calc(MINUS,left,right);
}

Token* XPathOperators::Operator_MULT(Token* left, Token* right)
{
	return operator_calc(MULT,left,right);
}

Token* XPathOperators::Operator_DIV(Token* left, Token* right)
{
	return operator_calc(DIV,left,right);
}

Token* XPathOperators::Operator_MOD(Token* left, Token* right)
{
	return operator_calc(MOD,left,right);
}

// Operator =
Token* XPathOperators::Operator_EQ(Token* left, Token* right)
{
	Token *t1 = left->GetType()<right->GetType()?left:right;
	Token *t2 = left->GetType()<right->GetType()?right:left;
	
	// Same type : string
	if(t1->GetType()==LIT_STR && t2->GetType()==LIT_STR)
		return new TokenBool(((TokenString *)t1)->s == ((TokenString *)t2)->s);
	
	// Same type : bool
	if(t1->GetType()==LIT_BOOL && t2->GetType()==LIT_BOOL)
		return new TokenBool(((TokenBool *)t1)->b == ((TokenBool *)t2)->b);
	
	// Same type : integer
	if(t1->GetType()==LIT_INT && t2->GetType()==LIT_INT)
		return new TokenBool(((TokenInt *)t1)->i == ((TokenInt *)t2)->i);
	
	// Same type : float
	if(t1->GetType()==LIT_FLOAT && t2->GetType()==LIT_FLOAT)
		return new TokenBool(((TokenFloat *)t1)->d == ((TokenFloat *)t2)->d);
	
	// Integer compared to something else (but not an integer)
	if(left->GetType()==LIT_INT || right->GetType()==LIT_INT)
	{
		try
		{
			// Try to cast as integer and compare
			return new TokenBool((int)(*left) == (int)(*right));
		}
		catch(...)
		{
			return new TokenBool(false);
		}
	}
	
	// Float compared to something else (but not a float)
	if(left->GetType()==LIT_FLOAT || right->GetType()==LIT_FLOAT)
	{
		try
		{
			// Try to cast as float and compare
			return new TokenBool((double)(*left) == (double)(*right));
		}
		catch(...)
		{
			return new TokenBool(false);
		}
	}
	
	// Literal comparted to sequence
	if((t1->GetType()==LIT_STR || t1->GetType()==LIT_INT || t1->GetType()==LIT_FLOAT) && t2->GetType()==SEQ)
	{
		TokenSeq *list = (TokenSeq *)t2;
		for(int i=0;i<list->items.size();i++)
		{
			try
			{
				if(t1->GetType()==LIT_STR && (string)(*list->items.at(i))==((TokenString *)t1)->s)
					return new TokenBool(true);
				else if(t1->GetType()==LIT_INT && (int)(*list->items.at(i))==((TokenInt *)t1)->i)
					return new TokenBool(true);
				else if(t1->GetType()==LIT_FLOAT && (double)(*list->items.at(i))==((TokenFloat *)t1)->d)
					return new TokenBool(true);
			}
			catch(...) {}
		}
		return new TokenBool(false);
	}
	
	// Two nodesets compared
	// TRUE if any of the elements of set1 is found in set2
	if(t1->GetType()==SEQ && t2->GetType()==SEQ)
	{
		TokenSeq *list1 = (TokenSeq *)t1;
		TokenSeq *list2 = (TokenSeq *)t2;
		for(int i=0;i<list1->items.size();i++)
		{
			for(int j=0;j<list2->items.size();j++)
				if((string)(*list1->items.at(i))==(string)(*list2->items.at(j)))
					return new TokenBool(true);
		}
		return new TokenBool(false);
	}
	
	// Operator could not be computed
	throw Exception("Operator EQ","Unable to evaluate operator '='");
}

// The exact reverse or Operator_EQ
Token* XPathOperators::Operator_NEQ(Token* left, Token* right)
{
	TokenBool *res = (TokenBool *)Operator_EQ(left,right);
	res->b = !res->b;
	return res;
}

// Comparison operators can always be computed on float
Token* XPathOperators::Operator_LT(Token* left, Token* right)
{
	return new TokenBool((double)(*left)<(double)(*right));
}

Token* XPathOperators::Operator_LEQ(Token* left, Token* right)
{
	return new TokenBool((double)(*left)<=(double)(*right));
}

Token* XPathOperators::Operator_GT(Token* left, Token* right)
{
	return new TokenBool((double)(*left)>(double)(*right));
}

Token* XPathOperators::Operator_GEQ(Token* left, Token* right)
{
	return new TokenBool((double)(*left)>=(double)(*right));
}

// Logical AND
Token* XPathOperators::Operator_AND(Token* left, Token* right)
{
	if(left->GetType()==LIT_BOOL && right->GetType()==LIT_BOOL)
		return new TokenBool(((TokenBool *)left)->b && ((TokenBool *)right)->b);
	
	throw Exception("Operator AND","Logical operator 'and' can  only operate on boolean values");
}

// Logical OR
Token* XPathOperators::Operator_OR(Token* left, Token* right)
{
	if(left->GetType()==LIT_BOOL && right->GetType()==LIT_BOOL)
		return new TokenBool(((TokenBool *)left)->b || ((TokenBool *)right)->b);
	
	throw Exception("Operator OR","Logical operator 'or' can  only operate on boolean values");
}