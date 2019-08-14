#include <string>
#include <vector>
#include <stack>
#include <DOM/DOMNamedNodeMap.h>
#include <XPath/XPathTokens.h>
#include <XPath/XPathParser.h>
#include <XPath/XPathEval.h>
using namespace std;

void dump_expr(TokenExpr *expr, int level);
void dump_token(Token *token,int level);

void dump_token(Token *token,int level)
{
	printf("%d %s\n",level,Token::ToString(token->GetType()).c_str());
	
	if(token->GetType()==FUNC)
	{
		TokenFunc *func = (TokenFunc *)token;
		for(int j=0;j<func->args.size();j++)
		{
			printf("arg %d :\n",j);
			dump_expr(func->args.at(j),0);
		}
		printf("end args\n");
	}
	else if(token->GetType()==NODENAME)
		TokenNodeName *node = (TokenNodeName *)token;
	else if(token->GetType()==SEQ)
	{
		TokenSeq *seq = (TokenSeq *)token;
		
		printf("Sequence %ld items\n",seq->items.size());
	}
	else if(token->GetType()==FILTER)
	{
		dump_expr(((TokenFilter *)token)->filter,0);
	}
	else if(token->GetType()==LIT_INT)
		printf("val = %d\n",((TokenInt *)token)->i);
	else if(token->GetType()==LIT_FLOAT)
		printf("val = %f\n",((TokenFloat *)token)->d);
	else if(token->GetType()==LIT_BOOL)
		printf("val = %s\n",((TokenBool *)token)->b?"true":"false");
	else if(token->GetType()==LIT_STR)
		printf("val = %s\n",((TokenString *)token)->s.c_str());
}

void dump_expr(TokenExpr *expr, int level)
{
	for(int i=0;i<expr->expr_tokens.size();i++)
	{
		dump_token(expr->expr_tokens.at(i),level);
		
		if(expr->expr_tokens.at(i)->GetType()==EXPR)
			dump_expr((TokenExpr *)expr->expr_tokens.at(i),level+1);
	}
}

/*int current_pos;
	Token *ptr;
	
	XQillaPlatformUtils::initialize();
	
	DOMDocument *xmldoc = DOMDocument::Parse("<root><task evqid='1' /><task evqid='4' /><job evqid='3'><task evqid='2' /></job></root>");
	//DOMDocument *xmldoc = DOMDocument::Parse("<output>3a</output>");
	
	//string expr = "count(/task[(@status='TERMINATED' and @retval = 0) and (@status='SKIPPED')])";
	//string expr = "//job[@name = 'job1']/following-sibling::*[name() = 'job']";
	string expr = "//*[]";
	//string expr = "following-sibling::job";
	//string expr = "/workflow/job[@name = 'job1']";
	
	try
	{
		XPathEval eval(xmldoc);
		Token *result = eval.Evaluate(expr,xmldoc->getDocumentElement());
		
		dump_token(result,0);
		//printf("%s\n",string(*result).c_str());
		//printf("%d\n",(int)(*result));
		
		delete result;
	}
	catch(Exception &excpt)
	{
		printf("Except : %s\n",excpt.error.c_str());
	}
	
	delete xmldoc;
	
	XQillaPlatformUtils::terminate();
	
	return 0;*/