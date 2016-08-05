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

#include <base64.h>

#include <string.h>

#include <sstream>

using namespace std;

static const char b64_table[64] = {
	'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
	'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',
	'0','1','2','3','4','5','6','7','8','9',
	'+','/'
};

bool base64_decode_file(FILE *f,const string &base64_str)
{
	unsigned char b64_rev_table[0x80];
	memset(b64_rev_table,'\xFF',64);
	for(int i=0;i<64;i++)
		b64_rev_table[b64_table[i]] = i;
	
	unsigned int block = 0, block_size = 0, block_padding = 0;
	char block_str[3];
	for(int i=0;i<base64_str.length();i++)
	{
		if(base64_str[i]>=0x80)
			return false; // Illegal character
		else if(base64_str[i]=='=')
		{
			// PAD
			block <<= 6;
			block_size++;
			block_padding++;
		}
		else if(base64_str[i]==' ' || base64_str[i]=='\r' || base64_str[i]=='\n' || base64_str[i]=='\t')
			continue; // Skip blanks
		else if(b64_rev_table[base64_str[i]]==0xFF)
			return false; // Illegal character
		else
		{
			block <<= 6;
			block |= b64_rev_table[base64_str[i]];
			
			block_size++;
		}
		
		if(block_size==4)
		{
			block_str[0] = (block & 0x00FF0000) >> 16;
			block_str[1] = (block & 0x0000FF00) >> 8;
			block_str[2] = (block & 0x000000FF);
			fwrite(block_str,1,3-block_padding,f);
			
			block = 0;
			block_size = 0;
			block_padding = 0;
		}
	}
	
	return true;
}

bool base64_decode_string(string &str,const string &base64_str)
{
	unsigned char b64_rev_table[0x80];
	memset(b64_rev_table,'\xFF',64);
	for(int i=0;i<64;i++)
		b64_rev_table[b64_table[i]] = i;
	
	unsigned int block = 0, block_size = 0, block_padding = 0;
	char block_str[3];
	for(int i=0;i<base64_str.length();i++)
	{
		if(base64_str[i]>=0x80)
			return false; // Illegal character
		else if(base64_str[i]=='=')
		{
			// PAD
			block <<= 6;
			block_size++;
			block_padding++;
		}
		else if(base64_str[i]==' ' || base64_str[i]=='\r' || base64_str[i]=='\n' || base64_str[i]=='\t')
			continue; // Skip blanks
		else if(b64_rev_table[base64_str[i]]==0xFF)
			return false; // Illegal character
		else
		{
			block <<= 6;
			block |= b64_rev_table[base64_str[i]];
			
			block_size++;
		}
		
		if(block_size==4)
		{
			block_str[0] = (block & 0x00FF0000) >> 16;
			block_str[1] = (block & 0x0000FF00) >> 8;
			block_str[2] = (block & 0x000000FF);
			str.append(block_str,3-block_padding);
			
			block = 0;
			block_size = 0;
			block_padding = 0;
		}
	}
	
	return true;
}

void base64_encode_file(FILE *f,string &base64_str)
{
	unsigned int block = 0, block_size = 0;
	char block_str[3];
	
	while(block_size = fread(block_str,1,3,f))
	{
		block = block_str[0] << 16;
		if(block_size>=1)
			block = block | (block_str[1] << 8);
		if(block_size>=2)
			block = block | block_str[2];
		
		base64_str.append(1,b64_table[(block & 0xFC0000) >> 18]);
		base64_str.append(1,b64_table[(block & 0x03F000) >> 12]);
		
		if(block_size==1)
			base64_str.append(2,'=');
		
		if(block_size==2)
		{
			base64_str.append(1,b64_table[(block & 0x000FC0) >> 6]);
			base64_str.append(1,'=');
		}
		
		if(block_size==3)
		{
			base64_str.append(1,b64_table[(block & 0x000FC0) >> 6]);
			base64_str.append(1,b64_table[(block & 0x00003F)]);
		}
	}
}

void base64_encode_string(const string &str,string &base64_str)
{
	unsigned int block = 0, block_size = 0;
	char block_str[3];
	
	istringstream input_stream(str);
	
	while(block_size = input_stream.readsome(block_str,3))
	{
		block = block_str[0] << 16;
		if(block_size>=1)
			block = block | (block_str[1] << 8);
		if(block_size>=2)
			block = block | block_str[2];
		
		base64_str.append(1,b64_table[(block & 0xFC0000) >> 18]);
		base64_str.append(1,b64_table[(block & 0x03F000) >> 12]);
		
		if(block_size==1)
			base64_str.append(2,'=');
		
		if(block_size==2)
		{
			base64_str.append(1,b64_table[(block & 0x000FC0) >> 6]);
			base64_str.append(1,'=');
		}
		
		if(block_size==3)
		{
			base64_str.append(1,b64_table[(block & 0x000FC0) >> 6]);
			base64_str.append(1,b64_table[(block & 0x00003F)]);
		}
	}
}