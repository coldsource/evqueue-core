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
#include <SocketSAX2Handler.h>

#include <string>
#include <map>
#include <vector>

class SocketQuerySAX2Handler : public SocketSAX2HandlerInterface {
	
	private:
		std::string group;
		
		bool ready;
		
		int level;
		
		WorkflowParameters params;
		std::vector<std::string> inputs;
		
	public:
		SocketQuerySAX2Handler(const std::string &context);
		SocketQuerySAX2Handler(const std::string &context, const std::string xml);
		~SocketQuerySAX2Handler();
		
		void startElement(const XMLCh* const uri, const XMLCh* const localname, const XMLCh* const qname, const xercesc::Attributes&  attrs );
		void endElement (const XMLCh *const uri, const XMLCh *const localname, const XMLCh *const qname);
		void endDocument();
		
		bool IsReady() { return ready; }
		const std::string &GetQueryGroup() { return group; }
		
		WorkflowParameters *GetWorkflowParameters() {return &params;};
		const std::vector<std::string> &GetInputs() { return inputs; }
};
