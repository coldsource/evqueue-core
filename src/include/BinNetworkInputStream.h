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

#ifndef _BIN_NETWORK_INPUT_STREAM_H_
#define _BIN_NETWORK_INPUT_STREAM_H_

#include <sys/types.h>
#include <xercesc/util/BinInputStream.hpp>

using namespace xercesc;

class BinNetworkInputStream : public BinInputStream
{
	protected:
		int socket;
		int pos;
	
	public:
		BinNetworkInputStream(int s);
		~BinNetworkInputStream();
		
		XMLFilePos curPos() const;
		XMLSize_t readBytes (XMLByte *const toFill, const XMLSize_t maxToRead);
		const XMLCh * getContentType() const;
};

#endif
