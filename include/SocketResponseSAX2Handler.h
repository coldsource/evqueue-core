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

using namespace xercesc;

#include <string>
#include <map>
#include <vector>

class SocketResponseSAX2Handler : public SocketSAX2HandlerInterface {
	
	private:
		std::string group;
		
		bool ready;
		
		int level;
	
	public:
		SocketResponseSAX2Handler(const std::string &context);
		~SocketResponseSAX2Handler();
		
		const std::string &GetGroup() { return group; }
		
		void startElement( const XMLCh* const uri, const XMLCh* const localname, const XMLCh* const qname, const Attributes&  attrs );
		void endElement (const XMLCh *const uri, const XMLCh *const localname, const XMLCh *const qname);
		void endDocument();
		
		bool IsReady() { return ready; }
};
