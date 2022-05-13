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

#ifndef _UTILS_DATE_H_
#define _UTILS_DATE_H_

#include <string>

#include <time.h>

namespace Utils
{
	
class Date
{
	public:
		static std::string FormatDate(const std::string &format, time_t ts = 0);
		static std::string ParseDate(const std::string &date, const std::string &in_format, const std::string out_format = "%Y-%m-%d %H:%M:%S");
		static std::string PastDate(int back_seconds, time_t from = 0);
};

}

#endif
