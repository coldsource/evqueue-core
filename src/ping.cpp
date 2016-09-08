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

#include <ping.h>

#include <SocketQuerySAX2Handler.h>
#include <QueryResponse.h>
#include <Configuration.h>

#include <xqilla/xqilla-dom3.hpp>

using namespace std;
using namespace xercesc;

bool ping_handle_query(const User &user, SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	const std::map<std::string,std::string> attrs = saxh->GetRootAttributes();
	
	response->GetDOM()->getDocumentElement()->setAttribute(X("version"),X(EVQUEUE_VERSION));
	response->GetDOM()->getDocumentElement()->setAttribute(X("node"),X(Configuration::GetInstance()->Get("cluster.node.name").c_str()));
	
	return true;
}
