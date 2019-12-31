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

#include <API/AuthHandler.h>
#include <Logger/Logger.h>
#include <Exception/Exception.h>
#include <API/Statistics.h>
#include <Configuration/ConfigurationEvQueue.h>
#include <User/User.h>
#include <User/Users.h>
#include <Crypto/Random.h>
#include <Crypto/Sha1String.h>
#include <Crypto/hmac.h>
#include <API/XMLQuery.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>

#include <iomanip>
#include <sstream>

using namespace std;

AuthHandler::AuthHandler()
{
	
}

void AuthHandler::SetRemote(const string &remote_host, int remote_port)
{
	this->remote_host = remote_host;
	this->remote_port = remote_port;
}

string AuthHandler::GetChallenge()
{
	challenge = generate_challenge();
	return challenge;
}

User AuthHandler::HandleChallenge(XMLQuery *query) const
{
	// Check response
	if(query->GetQueryGroup()!="auth")
		throw Exception("Authentication Handler","Expected 'auth' node","AUTH_ERROR");
	
	const string response = query->GetRootAttribute("response");
	const string user_name = query->GetRootAttribute("user");
	
	User user;
	
	// Check identification
	try
	{
		user = Users::GetInstance()->Get(user_name);
	}
	catch(Exception &e)
	{
		// Cach exceptions to hide real error for security reasons
		Logger::Log(LOG_NOTICE,"Authentication failed : unknown user "+user.GetName());
		throw Exception("Authentication Handler","Invalid authentication","AUTH_ERROR");
	}
	
	string hmac = hash_hmac(user.GetPassword(),challenge);
	if(time_constant_strcmp("hmac",response)!=0 && time_constant_strcmp(hmac,response)!=0)
	{
		Logger::Log(LOG_NOTICE,"Authentication failed for user '"+user.GetName()+"' : wrong password");
		throw Exception("Authentication Handler","Invalid authentication","AUTH_ERROR");
	}
	
	return user;
}

string AuthHandler::generate_challenge() const
{
	Sha1String sha1;
	sha1_ctx ctx;
	
	struct timeval tv;
	gettimeofday(&tv,0);
	sha1.ProcessBytes((void *)&tv.tv_sec,sizeof(timeval::tv_sec));
	sha1.ProcessBytes((void *)&tv.tv_usec,sizeof(timeval::tv_usec));
	
	string random_kernel;
	try
	{
		random_kernel = Random::GetInstance()->GetKernelRandomBinary(6);
	}
	catch(Exception &e)
	{
		throw Exception("Authentication Handler","Internal error","AUTH_ERROR");
	}
	
	sha1.ProcessBytes(random_kernel);
	
	string random_mt =  Random::GetInstance()->GetMTRandomBinary(16);
	sha1.ProcessBytes(random_mt);
	
	static clock_t base_time = clock();
	clock_t t = clock()-base_time;
	sha1.ProcessBytes((void *)&t,sizeof(clock_t));
	
	Statistics *stats = Statistics::GetInstance();
	unsigned int accepted_cnx = stats->GetAcceptedConnections();
	sha1.ProcessBytes((void *)&accepted_cnx,sizeof(unsigned int));
	
	sha1.ProcessBytes(remote_host);
	sha1.ProcessBytes((void *)&remote_port,sizeof(int));
	
	return sha1.GetHex();
}

int AuthHandler::time_constant_strcmp(const std::string &str1, const std::string &str2) const
{
	if(str1.length()!=str2.length())
		return 1;
	
	unsigned char result = 0;
	for(int i=0;i<str1.length();i++)
		result |= str1[i] ^ str2[i];
	
	return result;
}
