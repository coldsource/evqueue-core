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

#include <Crypto/Random.h>
#include <Exception/Exception.h>

#include <stdio.h>

using namespace std;

Random *Random::instance = 0;

Random::Random()
{
	string seed_str = GetKernelRandomBinary(32);
	seed_seq mt_seed (seed_str.begin(),seed_str.end());
	mt_generator.seed(mt_seed);
	
	instance = this;
}

string Random::GetKernelRandomBinary(int length)
{
	if(length>1024)
		throw Exception("Random", "Could read as much random");
	
	char random_buf[1024];
	FILE *f = fopen("/dev/urandom","r");
	if(!f || fread(random_buf,1,length,f)!=length)
		throw Exception("Random","Unable to read /dev/urandom");
	
	fclose(f);
	
	return string(random_buf,length);
}

string Random::GetMTRandomBinary(int length)
{
	string random_str;
	uniform_int_distribution<> dis(0, 255);
	for(int i=0;i<length;i++)
		random_str.append(1,(char)dis(mt_generator));
	
	return random_str;
}
