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
#include <sha1.h>

#include <iomanip>
#include <sstream>

using namespace std;

string hash_hmac(const string &key, const string &data)
{
	string ikey, okey;
	
	//  Prepare inner and outer key by adding padding
	for(int i=0;i<64;i++)
	{
		if(i<key.length())
		{
			ikey.append(1,key[i]^0x36);
			okey.append(1,key[i]^0x5c);
		}
		else
		{
			ikey.append(1,0x36);
			okey.append(1,0x5c);
		}
	}
	
	// Compute inner hash
	sha1_ctx ctx;
	char c_hash[20];
	
	sha1_init_ctx(&ctx);
	sha1_process_bytes(ikey.c_str(),ikey.length(),&ctx);
	sha1_process_bytes(data.c_str(),data.length(),&ctx);
	sha1_finish_ctx(&ctx,c_hash);
	
	// Compute outer hash, that is HMAC
	sha1_init_ctx(&ctx);
	sha1_process_bytes(okey.c_str(),okey.length(),&ctx);
	sha1_process_bytes(c_hash,20,&ctx);
	sha1_finish_ctx(&ctx,c_hash);
	
	// Format HEX result
	stringstream sstream;
	sstream << hex;
	for(int i=0;i<20;i++)
		sstream << std::setw(2) << setfill('0') << (int)(c_hash[i]&0xFF);
	return sstream.str();
}