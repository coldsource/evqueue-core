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

#ifndef _EXCEPTIONMANAGER_H_
#define _EXCEPTIONMANAGER_H_

class Exception;

class ExceptionManager
{
	static thread_local Exception *current_exception;
	static thread_local bool exception_logged;
	
public:
	static void RegisterException(Exception *e);
	static void UnregisterException(Exception *e);
	
	static Exception *GetCurrentException() { return current_exception; }
	
	static bool IsExceptionLogged() { return exception_logged; }
	static void SetExceptionLogged() { exception_logged = true; }
};

#endif