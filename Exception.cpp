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

#include <Exception.h>
#include <string.h>

Exception::Exception(const char *context,const char *error)
{
	int len;
	
	len=strlen(context);
	this->context=new char[len+1];
	memcpy(this->context,context,len+1);
	
	len=strlen(error);
	this->error=new char[len+1];
	memcpy(this->error,error,len+1);
}

Exception::Exception(const Exception &e)
{
	int len;
	
	len=strlen(e.context);
	this->context=new char[len+1];
	memcpy(this->context,e.context,len+1);
	
	len=strlen(e.error);
	this->error=new char[len+1];
	memcpy(this->error,e.error,len+1);
}

Exception::~Exception(void)
{
	delete[] context;
	delete[] error;
}

Exception & Exception::operator=(const Exception &e)
{
	delete[] context;
	delete[] error;
	
	int len;
	
	len=strlen(e.context);
	this->context=new char[len+1];
	memcpy(this->context,e.context,len+1);
	
	len=strlen(e.error);
	this->error=new char[len+1];
	memcpy(this->error,e.error,len+1);
}
