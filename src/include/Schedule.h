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

#ifndef _SCHEDULE_H_
#define _SCHEDULE_H_

#define SCHEDULE_LEVEL_SEC         0
#define SCHEDULE_LEVEL_MIN         1
#define SCHEDULE_LEVEL_HOUR        2
#define SCHEDULE_LEVEL_DAY         3
#define SCHEDULE_LEVEL_MONTH       4
#define SCHEDULE_LEVEL_WDAY        5
#define SCHEDULE_LENGTH            194

#include <time.h>

#include <string>

class Schedule
{
	bool schedule[SCHEDULE_LENGTH];
	
	public:
		Schedule() {}
		Schedule(const std::string &schedule_description_s);
		
		static int GetScheduleLength(int level);
		static int GetScheduleOffset(int level);
		
		time_t GetNextTime(void);
	
	private:
		time_t get_next_time(time_t now);
};

#endif
