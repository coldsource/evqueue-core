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

#include <global.h>
#include <Workflow/WorkflowParameters.h>
#include <API/SocketSAX2Handler.h>

#include <string>
#include <map>
#include <vector>

#include <xercesc/sax2/DefaultHandler.hpp>
#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMText.hpp>
#include <DOM/DOMDocument.h>

class SocketResponseSAX2Handler : public SocketSAX2HandlerInterface {
	
	private:
		bool record;
		DOMDocument xmldoc;
		std::vector<DOMElement> current_node;
		DOMText current_text_node;
		
		
		std::string group;
		
		bool ready;
		int level;
	
	public:
		SocketResponseSAX2Handler(const std::string &context, bool record = false);
		~SocketResponseSAX2Handler();
		
		const std::string &GetGroup() { return group; }
		DOMDocument *GetDOM() { return &xmldoc; }
		
		void startElement( const XMLCh* const uri, const XMLCh* const localname, const XMLCh* const qname, const xercesc::Attributes&  attrs );
		void endElement (const XMLCh *const uri, const XMLCh *const localname, const XMLCh *const qname);
		void characters(const XMLCh *const chars, const XMLSize_t length);
		void endDocument();
		
		bool IsReady() { return ready; }
};
