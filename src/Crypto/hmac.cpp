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

#include <Crypto/hmac.h>
#include <Crypto/Sha1String.h>
#include <Exception/Exception.h>

using namespace std;

static int hexchar2bin(char c)
{
	if(c>='0' && c<='9')
		return c-'0';
	else if(c>='a' && c<='f')
		return c-'a'+10;
	else if(c>='A' && c<='F')
		return c-'A'+10;
	else
		throw Exception("hmac","Invalid hexadecimal string");
}

static string hex2bin(const string &hex)
{
	if(hex.length()%2!=0)
		throw Exception("hmac","Invalid hexadecimal string");
	
	string bin;
	for(int i=0;i<hex.length();i+=2)
	{
		char c = ((hexchar2bin(hex.at(i))&0x0F)<<4) | (hexchar2bin(hex.at(i+1))&0x0F);
		bin.append(1,c);
	}
	
	return bin;
}

string hash_hmac(const string &key, const string &data, bool binary)
{
	// Decode input hexadecimal strings to binary
	string bkey = binary?key:hex2bin(key);
	string bdata = binary?data:hex2bin(data);
	
	string ikey, okey;
	
	//  Prepare inner and outer key by adding padding
	for(int i=0;i<64;i++)
	{
		if(i<bkey.length())
		{
			ikey.append(1,bkey[i]^0x36);
			okey.append(1,bkey[i]^0x5c);
		}
		else
		{
			ikey.append(1,0x36);
			okey.append(1,0x5c);
		}
	}
	
	// Compute inner hash
	Sha1String isha1;
	isha1.ProcessBytes(ikey);
	isha1.ProcessBytes(bdata);
	
	// Compute outer hash
	Sha1String osha1;
	osha1.ProcessBytes(okey);
	osha1.ProcessBytes(isha1.GetBinary());
	
	return binary?osha1.GetBinary():osha1.GetHex();
}
