#ifndef _DOMXPATHRESULT_H_
#define _DOMXPATHRESULT_H_

#include <xercesc/dom/DOM.hpp>
#include <DOMNode.h>

class DOMXPathResult
{
	xercesc::DOMXPathResult *result;
	
public:
	enum ResultType {
		ANY_TYPE = 0, NUMBER_TYPE = 1, STRING_TYPE = 2, BOOLEAN_TYPE = 3,
		UNORDERED_NODE_ITERATOR_TYPE = 4, ORDERED_NODE_ITERATOR_TYPE = 5, UNORDERED_NODE_SNAPSHOT_TYPE = 6, ORDERED_NODE_SNAPSHOT_TYPE = 7,
		ANY_UNORDERED_NODE_TYPE = 8, FIRST_ORDERED_NODE_TYPE = 9, FIRST_RESULT_TYPE = 100, ITERATOR_RESULT_TYPE = 101,
		SNAPSHOT_RESULT_TYPE = 102
	};
	
	DOMXPathResult(xercesc::DOMXPathResult *result);
	~DOMXPathResult();

	bool snapshotItem(int index);
	
	bool isNode();
	DOMNode getNodeValue();
	int getIntegerValue();
};

#endif