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

#include <BinNetworkInputStream.h>
#include <Exception.h>

#include <sys/socket.h>
#include <string.h>
#include <errno.h>

#include <string>

using namespace std;

BinNetworkInputStream::BinNetworkInputStream(int s)
{
	socket = s;
	pos = 0;
}

BinNetworkInputStream::~BinNetworkInputStream()
{
	
}

XMLFilePos BinNetworkInputStream::curPos() const
{
	return pos;
}

XMLSize_t BinNetworkInputStream::readBytes(XMLByte *const toFill, const XMLSize_t maxToRead)
{
	int read_size = recv(socket,toFill,maxToRead,0);
	if(read_size==-1)
		throw Exception("BinNetworkInputStream","Error reading socket, recv() returned error "+to_string(errno));

	pos += read_size;
	return (XMLSize_t) read_size;
}

const XMLCh * BinNetworkInputStream::getContentType() const
{
	return 0;
}
