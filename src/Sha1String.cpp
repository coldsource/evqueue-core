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
#include <Sha1String.h>

#include <iomanip>
#include <sstream>

using namespace std;

Sha1String::Sha1String()
{
	sha1_init_ctx(&ctx);
}

Sha1String::Sha1String(const std::string &str)
{
	sha1_init_ctx(&ctx);
	
	ProcessBytes(str);
}

void Sha1String::ProcessBytes(const std::string &str)
{
	sha1_process_bytes(str.c_str(),str.length(),&ctx);
}

void Sha1String::ProcessBytes(void *bytes, int length)
{
	sha1_process_bytes(bytes,length,&ctx);
}

std::string Sha1String::GetBinary()
{
	char c_hash[20];
	sha1_finish_ctx(&ctx,c_hash);
	
	string hash_str;
	hash_str.append(c_hash,20);
	
	return hash_str;
}

std::string Sha1String::GetHex()
{
	char c_hash[20];
	sha1_finish_ctx(&ctx,c_hash);
	
	stringstream sstream;
	sstream << hex;
	for(int i=0;i<20;i++)
		sstream << std::setw(2) << setfill('0') << (int)(c_hash[i]&0xFF);
	return sstream.str();
}