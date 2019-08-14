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

#include <Exception/ExceptionManager.h>
#include <Exception/Exception.h>

thread_local Exception *ExceptionManager::current_exception = 0;
thread_local bool ExceptionManager::exception_logged = false;

void ExceptionManager::RegisterException(Exception *e)
{
	current_exception = e;
	exception_logged = false;
}

void ExceptionManager::UnregisterException(Exception *e)
{
	if(current_exception==e)
		current_exception = 0;
}