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

#include <Cluster/Cluster.h>
#include <Configuration/ConfigurationEvQueue.h>
#include <Exception/Exception.h>
#include <API/SocketSAX2Handler.h>
#include <Logger/Logger.h>
#include <API/Client.h>
#include <Crypto/sha1.h>

#include <sstream>
#include <iomanip>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <netdb.h>

using namespace std;

Cluster *Cluster::instance = 0;

Cluster::Cluster(const string &conf)
{
	instance = this;
	
	notify = ConfigurationEvQueue::GetInstance()->GetBool("cluster.notify");
	
	istringstream split(conf);
	for (std::string each; std::getline(split, each, ','); nodes.push_back(each));
	
	if(notify && nodes.size())
		Logger::Log(LOG_NOTICE,"Cluster notifications enabled");
	
	user = ConfigurationEvQueue::GetInstance()->Get("cluster.notify.user");
	password = ClientBase::HashPassword(ConfigurationEvQueue::GetInstance()->Get("cluster.notify.password"));
	
	cnx_timeout = ConfigurationEvQueue::GetInstance()->GetInt("cluster.cnx.timeout");
	snd_timeout = ConfigurationEvQueue::GetInstance()->GetInt("cluster.snd.timeout");
	rcv_timeout = ConfigurationEvQueue::GetInstance()->GetInt("cluster.rcv.timeout");
}

void Cluster::Notify(const string &command)
{
	if(!notify)
		return; // Cluster notifications are disabled
	
	ExecuteCommand(command);
}

bool Cluster::ExecuteCommand(const string &command, DOMDocument *response )
{
	if(nodes.size()==0)
		return false; // Cluster is not configured
	
	Logger::Log(LOG_NOTICE, "Executing cluster command "+command);
	
	if(response)
		response->appendChild(response->createElement("cluster-response"));
	
	for(int i=0;i<nodes.size();i++)
	{
		try
		{
			Client client(nodes.at(i),user,password);
			client.SetTimeouts(cnx_timeout,snd_timeout,rcv_timeout);
			
			// Check if we are not self connecting
			const string node = client.Connect();
			if(node==ConfigurationEvQueue::GetInstance()->Get("cluster.node.name"))
			{
				client.Disconnect();
				Logger::Log(LOG_INFO, "Skipping current node in cluster notification");
				continue;
			}
			
			client.Exec(command);
			
			if(response)
				response->getDocumentElement().appendChild(response->importNode(client.GetResponseDOM()->getDocumentElement(),true));
		}
		catch(Exception &e)
		{
			Logger::Log(LOG_ERR, "Error executing cluster command on node "+nodes.at(i)+" : "+e.error);
		}
	}
	
	return true;
}

std::vector<std::string> Cluster::Ping(void)
{
	Logger::Log(LOG_INFO, "Discovering cluster");
	
	vector<string> res;
	
	for(int i=0;i<nodes.size();i++)
	{
		try
		{
			Client client(nodes.at(i),user,password);
			client.SetTimeouts(cnx_timeout,snd_timeout,rcv_timeout);
			
			res.push_back(client.Connect());
			client.Disconnect();
		}
		catch(Exception &e)
		{
			// Errors are ignored in discovery mode
		}
	}
	
	return res;
}
