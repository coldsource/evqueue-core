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

#include <API/APISession.h>
#include <Logger/Logger.h>
#include <API/QueryResponse.h>
#include <API/AuthHandler.h>
#include <Configuration/ConfigurationEvQueue.h>
#include <API/QueryHandlers.h>
#include <Exception/Exception.h>
#include <API/XMLQuery.h>
#include <API/Statistics.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <memory>

using namespace std;

APISession::APISession(const string &context, int s)
{
	this->s = s;
	this->wsi = 0;
	
	init(context);
}

APISession::APISession(const string &context, struct lws *wsi)
{
	this->s = lws_get_socket_fd(wsi);
	this->wsi = wsi;
	
	init(context);
}

void APISession::init(const std::string& context)
{
	status=INITIALIZED;
	this->context = context;
	
	socklen_t remote_addr_len;
	struct sockaddr_in remote_addr;
	char remote_addr_str[16] = "local";
	remote_addr_len = sizeof(remote_addr);
	getpeername(s,(struct sockaddr*)&remote_addr,&remote_addr_len);
	if(remote_addr.sin_family==AF_INET)
	{
		inet_ntop(AF_INET,&(remote_addr.sin_addr),remote_addr_str,16);
		remote_port = ntohs(remote_addr.sin_port);
	}
	
	remote_host = remote_addr_str;
	
	ah.SetRemote(remote_host,remote_port);
	
	Logger::Log(LOG_INFO,"Accepted "+context+" connection from "+remote_host+":"+to_string(remote_port));
}

void APISession::Acquire()
{
	// Notify one thread is using this session
	unique_lock<mutex> llock(lock);
	
	acquired++;
}

bool APISession::Release()
{
	unique_lock<mutex> llock(lock);
	
	acquired--;
	
	if(acquired==0 && deleted)
		return true; // Flagged as deleted and thread was last one to use this session, it must delete it
	
	return false;
}

bool APISession::FlagDeleted()
{
	unique_lock<mutex> llock(lock);
	
	if(acquired==0)
		return false; // Session is not currently used, no need to flag as deleted, it must be deleted now
	
	deleted = true; // Flagged for removing after last use
	return true;
}

void APISession::SendChallenge()
{
	// Check if authentication is required
	if(!ConfigurationEvQueue::GetInstance()->GetBool("core.auth.enable"))
	{
		user = User::anonymous;
		status = AUTHENTICATED;
		return;
	}
	
	QueryResponse response("auth");
	if(wsi)
		response.SetWebsocket(wsi);
	else
		response.SetSocket(s);
	
	response.SetAttribute("challenge",ah.GetChallenge());
	response.SendResponse();
	
	status = WAITING_CHALLENGE_RESPONSE;
}

void APISession::ChallengeReceived(XMLQuery *query)
{
	user = ah.HandleChallenge(query);
	
	status = AUTHENTICATED;
	
	Logger::Log(LOG_INFO,"Successful authentication of user '%s'",user.GetName().c_str());
}

void APISession::SendGreeting()
{
	// Compute current time
	time_t t;
	struct tm t_desc;

	t = time(0);
	localtime_r(&t,&t_desc);

	char time_str[64];
	sprintf(time_str,"%d-%02d-%02d %02d:%02d:%02d",1900+t_desc.tm_year,t_desc.tm_mon+1,t_desc.tm_mday,t_desc.tm_hour,t_desc.tm_min,t_desc.tm_sec);
	
	QueryResponse ready_response("ready");
	if(wsi)
		ready_response.SetWebsocket(wsi);
	else
		ready_response.SetSocket(s);
	
	ready_response.SetAttribute("profile",user.GetProfile());
	ready_response.SetAttribute("version",EVQUEUE_VERSION);
	ready_response.SetAttribute("node",ConfigurationEvQueue::GetInstance()->Get("cluster.node.name"));
	ready_response.SetAttribute("time",time_str);
	
	// Send list of registered modules and their versions
	auto node_modules = ready_response.AppendXML("<modules />");
	auto modules = QueryHandlers::GetInstance()->GetModules();
	for(auto it = modules.begin(); it!=modules.end();++it)
	{
		DOMElement node_module = (DOMElement)ready_response.AppendXML("<module />", node_modules);
		node_module.setAttribute("name", it->first);
		node_module.setAttribute("version", it->second);
	}
	
	ready_response.SendResponse();
	
	status = READY;
}

bool APISession::QueryReceived(XMLQuery *query, int external_id, const string &object_id, unsigned long long event_id)
{
	if(wsi)
		Statistics::GetInstance()->IncWSQueries();
	else
		Statistics::GetInstance()->IncAPIQueries();
	
	QueryResponse response;
	if(wsi!=0)
		response.SetWebsocket(wsi);
	else if(s!=-1)
		response.SetSocket(s);
	
	if(query->GetQueryGroup()=="quit")
	{
		Logger::Log(LOG_DEBUG,"API : Received quit command, exiting channel");
		return true;
	}
	
	try
	{
		if(!QueryHandlers::GetInstance()->HandleQuery(user, query->GetQueryGroup(),query, &response))
			throw Exception("API","Unknown command or action","UNKNOWN_COMMAND");
	}
	catch (Exception &e)
	{
		Logger::Log(LOG_DEBUG,"Exception in context "+e.context+" : "+e.error);
		
		response.SetError(e.error);
		response.SetErrorCode(e.code);
		responses.push(move(response));
		return false;
	}
	
	Logger::Log(LOG_DEBUG,"API : Successfully called, sending response");
	
	// Apply XPath filter if requested and store response
	unique_lock<mutex> llock(lock);
	
	string xpath = query->GetRootAttribute("xpathfilter","");
	if(xpath!="")
	{
		unique_ptr<DOMXPathResult> res(response.GetDOM()->evaluate(xpath,response.GetDOM()->getDocumentElement(),DOMXPathResult::FIRST_RESULT_TYPE));
		
		QueryResponse xpath_response;
		xpath_response.GetDOM()->ImportXPathResult(res.get(),xpath_response.GetDOM()->getDocumentElement());
		
		responses.push(move(xpath_response));
	}
	else
		responses.push(move(response));
	
	if(external_id)
		responses.back().SetAttribute("external-id",to_string(external_id));
	if(object_id!="")
		responses.back().SetAttribute("object-id",object_id);
	if(event_id)
		responses.back().SetAttribute("event-id",to_string(event_id));
	
	return false;
}

bool APISession::SendResponse()
{
	unique_lock<mutex> llock(lock);
	
	if(responses.size()==0)
		return false; // No queued responses
	
	QueryResponse &response = responses.front();
	response.SendResponse();
	
	responses.pop();
	
	return true;
}

void APISession::Query(const string &xml,int external_id, const string &object_id, unsigned long long event_id)
{
	XMLQuery query(context,xml);
	QueryReceived(&query, external_id, object_id, event_id);
	SendResponse();
}
