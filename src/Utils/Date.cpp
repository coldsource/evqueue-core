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

#include <Utils/Date.h>
#include <Exception/Exception.h>

#include <string.h>

using namespace std;

namespace Utils
{

string Date::FormatDate(const string &format, time_t ts)
{
	char buf[32];
	if(ts==0)
		ts = time(0);
	
	struct tm ts_t;
	localtime_r(&ts, &ts_t);
	
	strftime(buf, 32, format.c_str(), &ts_t);
	return string(buf);
}

string Date::ParseDate(const string &date, const string &in_format, const string out_format)
{
	// Parse date, based on specified format
	struct tm date_t;
	memset(&date_t, 0, sizeof(struct tm));
	
	// Get raw date and parse it
	if(!strptime(date.c_str(), in_format.c_str(), &date_t))
		throw Exception("Utils::Date", "Unable to parse date : "+date);
	
	// Normalize date
	char buf[32];
	strftime(buf, 32, out_format.c_str(), &date_t);
	return string(buf);
}

string Date::PastDate(int back_seconds, time_t from)
{
	time_t past;
	struct tm past_t;
	
	if(from==0)
		from = time(0);
	
	return FormatDate("%Y-%m-%d %H:%M:%S", from - back_seconds);
}

}
