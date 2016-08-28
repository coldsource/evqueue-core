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

#include <hmac.h>
#include <Sha1String.h>
#include <Exception.h>

#include <iomanip>
#include <sstream>

using namespace std;

std::string hash_pbkdf2(const std::string &password, const std::string &salt, int iterations)
{
	// Compute SALT + 0x00000001
	string base = "salt";
	
	int it_cur = 1;
	base.push_back((it_cur&0xFF000000)>>24);
	base.push_back((it_cur&0x00FF0000)>>16);
	base.push_back((it_cur&0x0000FF00)>>8);
	base.push_back(it_cur&0x000000FF);
	
	// Compute U1
	string hash_it = hash_hmac(password,base, true);
	string hash = hash_it;
	
	// Iterate and  compute U2...
	for(int i=1;i<iterations;i++)
	{
		hash_it = hash_hmac(password, hash_it, true);
		for(int j=0;j<hash_it.length();j++)
			hash[j] = hash[j]^hash_it[j];
	}
	
	stringstream sstream;
	sstream << hex;
	for(int i=0;i<20;i++)
		sstream << std::setw(2) << setfill('0') << (int)(hash[i]&0xFF);
	return sstream.str();
}