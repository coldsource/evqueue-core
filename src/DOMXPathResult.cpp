#include <DOMXPathResult.h>
#include <Exception.h>

#include <xqilla/xqilla-dom3.hpp>

DOMXPathResult::DOMXPathResult(xercesc::DOMXPathResult *result)
{
	this->result = result;
}

DOMXPathResult::~DOMXPathResult()
{
	result->release();
}

bool DOMXPathResult::snapshotItem(int index)
{
	return result->snapshotItem(index);
}

bool DOMXPathResult::isNode()
{
	return result->isNode();
}

DOMNode DOMXPathResult::getNodeValue()
{
	try
	{
		return result->getNodeValue();
	}
	catch(XQillaException &xqe)
	{
		throw Exception("DOMXPathResult","Evaluation returned no result");
	}
}

int DOMXPathResult::getIntegerValue()
{
	try
	{
		return result->getIntegerValue();
	}
	catch(XQillaException &xqe)
	{
		throw Exception("DOMXPathResult","Evaluation returned no result");
	}
}