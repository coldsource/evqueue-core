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

bool base64_decode_file(FILE *f,const char *base64_str)
{
	static const char b64_table[64] = {
		'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
		'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',
		'0','1','2','3','4','5','6','7','8','9',
		'+','/'
	};
	
	unsigned char b64_rev_table[0x80];
	memset(b64_rev_table,'\xFF',64);
	for(int i=0;i<64;i++)
		b64_rev_table[b64_table[i]] = i;
	
	const char *ptr = base64_str;
	unsigned int block = 0, block_size = 0;
	char block_str[3];
	while(ptr[0])
	{
		if(ptr[0]>=0x80)
			return false; // Illegal character
		else if(ptr[0]=='=')
		{
			// PAD
			block <<= 6;
			block_size++;
		}
		else if(ptr[0]==' ' || ptr[0]=='\r' || ptr[0]=='\n' || ptr[0]=='\t')
		{
			ptr++;
			continue; // Skip blanks
		}
		else if(b64_rev_table[ptr[0]]==0xFF)
			return false; // Illegal character
		else
		{
			block <<= 6;
			block |= b64_rev_table[ptr[0]];
			
			block_size++;
		}
		
		if(block_size==4)
		{
			block_str[0] = (block & 0x00FF0000) >> 16;
			block_str[1] = (block & 0x0000FF00) >> 8;
			block_str[2] = (block & 0x000000FF);
			fwrite(block_str,1,3,f);
			
			block = 0;
			block_size = 0;
		}
		
		ptr++;
	}
	
	return true;
}