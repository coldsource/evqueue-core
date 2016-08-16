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

#include <AuthHandler.h>
#include <Logger.h>
#include <SocketSAX2Handler.h>
#include <SocketResponseSAX2Handler.h>
#include <Exception.h>
#include <Statistics.h>
#include <sha1.h>
#include <hmac.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>

#include <iomanip>
#include <sstream>

using namespace std;

AuthHandler::AuthHandler(int socket)
{
	this->socket = socket;
}

void AuthHandler::HandleAuth()
{
	string challenge = generate_challenge();
	string challenge_xml = "<auth challenge='"+challenge+"' />\n";
	send(socket,challenge_xml.c_str(),challenge_xml.length(),0);
	
	try
	{
		SocketResponseSAX2Handler saxh("Authentication Handler");
		SocketSAX2Handler socket_sax2_handler(socket);
		socket_sax2_handler.HandleQuery(&saxh);
		
		if(saxh.GetGroup()!="auth")
			throw Exception("Authentication Handler","Expected 'auth' node");
		
		const string response = saxh.GetRootAttribute("response");
		
		string hmac = hash_hmac("",challenge);
		if(time_constant_strcmp("hmac",response)!=0)
			throw Exception("Authentication Handler","Invalid authentication");
	}
	catch(Exception &e)
	{
		throw e;
	}
}

string AuthHandler::generate_challenge()
{
	sha1_ctx ctx;
	sha1_init_ctx(&ctx);
	
	struct timeval tv;
	gettimeofday(&tv,0);
	sha1_process_bytes((void *)&tv.tv_sec,sizeof(timeval::tv_sec),&ctx);
	sha1_process_bytes((void *)&tv.tv_usec,sizeof(timeval::tv_usec),&ctx);
	
	char devrandom[6];
	FILE *f = fopen("/dev/urandom","r");
	if(!f || fread(devrandom,1,6,f)!=6)
	{
		Logger::Log(LOG_ERR,"Unable to read /dev/urandom while trying to generate challenge");
		throw Exception("Authentication Handler","Internal error");
	}
	fclose(f);
	
	static clock_t base_time = clock();
	clock_t t = clock()-base_time;
	sha1_process_bytes((void *)&t,sizeof(clock_t),&ctx);
	
	Statistics *stats = Statistics::GetInstance();
	unsigned int accepted_cnx = stats->GetAcceptedConnections();
	sha1_process_bytes((void *)&accepted_cnx,sizeof(unsigned int),&ctx);
	
	char c_hash[20];
	sha1_finish_ctx(&ctx,c_hash);
	
	stringstream sstream;
	sstream << hex;
	for(int i=0;i<20;i++)
		sstream << std::setw(2) << setfill('0') << (int)(c_hash[i]&0xFF);
	return sstream.str();
}

int AuthHandler::time_constant_strcmp(const std::string &str1, const std::string &str2)
{
	if(str1.length()!=str2.length())
		return 1;
	
	unsigned char result = 0;
	for(int i=0;i<str1.length();i++)
		result |= str1[i] ^ str2[i];
	
	return result;
}