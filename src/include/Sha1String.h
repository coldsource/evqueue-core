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

#ifndef _SHA1STRING_H_
#define _SHA1STRING_H_

#include <sha1.h>

#include <string>

class Sha1String
{
	sha1_ctx ctx;
	
	public:
		Sha1String();
		Sha1String(const std::string &str);
		
		void ProcessBytes(const std::string &str);
		void ProcessBytes(void *bytes, int length);
		
		std::string GetBinary();
		std::string GetHex();
};

#endif