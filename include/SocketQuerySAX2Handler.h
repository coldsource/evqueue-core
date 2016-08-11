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
#include <WorkflowParameters.h>
#include <xercesc/sax2/DefaultHandler.hpp>

using namespace xercesc;

#include <string>
#include <map>
#include <vector>

class SocketQuerySAX2Handler : public DefaultHandler {
	
	private:
		std::string group;
		std::map<std::string,std::string> root_attributes;
		
		bool ready;
		
		int level;
		
		WorkflowParameters params;
		std::vector<std::string> inputs;
		
	public:
		SocketQuerySAX2Handler();
		~SocketQuerySAX2Handler();
		
		void startElement( const XMLCh* const uri, const XMLCh* const localname, const XMLCh* const qname, const Attributes&  attrs );
		void endElement (const XMLCh *const uri, const XMLCh *const localname, const XMLCh *const qname);
		void endDocument();
		
		bool IsReady() { return ready; }
		const std::string &GetQueryGroup() { return group; }
		const std::map<std::string,std::string> &GetRootAttributes() { return root_attributes; }
		
		WorkflowParameters *GetWorkflowParameters() {return &params;};
		const std::vector<std::string> &GetInputs() { return inputs; }
};
