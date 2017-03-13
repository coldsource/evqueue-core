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

#include <Schedule.h>
#include <Exception.h>

#include <string.h>
#include <stdlib.h>

#include <stdio.h>

using namespace std;

Schedule::Schedule(const string &schedule_description_s)
{
	const char *schedule_description = schedule_description_s.c_str();
	
	// Parse schedule description
	memset(&schedule,0,SCHEDULE_LENGTH);
	
	const char *start_ptr;
	char *end_ptr;
	int i = 0, level = 0, offset = 0, length = 0, min_day = 32;
	do
	{
		offset += length;
		length = GetScheduleLength(level);
		
		bool empty_level = 1;
		while(schedule_description[i]!=';' && schedule_description[i]!='\0')
		{
			start_ptr = schedule_description+i;
			long v = strtol(start_ptr,&end_ptr,10);
			
			if(start_ptr==end_ptr)
				throw Exception("Schedule","Non numeric data in schedule offset");
			
			if(v<0 || v>=length)
				throw Exception("Schedule","Out of range schedule offset");
			
			schedule[offset+v] = true;
			empty_level = false;
			
			if(level==SCHEDULE_LEVEL_DAY && v<min_day)
				min_day = v;
			
			i += end_ptr-start_ptr;
			
			if(schedule_description[i]==',')
				i++;
		}
		
		if(empty_level) // If no schedule is required at a specific level, we consider a full level
			for(int i=offset;i<offset+length;i++)
				schedule[i] = true;
		
		if(schedule_description[i]==';')
				i++;
		
		level++;
	}while(level<=SCHEDULE_LEVEL_WDAY);
	
	if(schedule_description[i]!='\0')
		throw Exception("Schedule","Garbage at end of schedule");
	
	if(min_day==29 && (!schedule[175] && !schedule[177] && !schedule[178] && !schedule[179] && !schedule[180] && !schedule[181] && !schedule[182] && !schedule[183] && !schedule[184] && !schedule[185] && !schedule[186]))
		throw Exception("Schedule","Schedule constraints are impossible to satisfy");
	
	if(min_day==30 && (!schedule[175] && !schedule[177] && !schedule[179] && !schedule[181] && !schedule[182] && !schedule[184] && !schedule[186]))
		throw Exception("Schedule","Schedule constraints are impossible to satisfy");
}

int Schedule::GetScheduleLength(int level)
{
	if(level==SCHEDULE_LEVEL_SEC)
		return 60;
	else if(level==SCHEDULE_LEVEL_MIN)
		return 60;
	else if(level==SCHEDULE_LEVEL_HOUR)
		return 24;
	else if(level==SCHEDULE_LEVEL_DAY)
		return 31;
	else if(level==SCHEDULE_LEVEL_MONTH)
		return 12;
	else if(level==SCHEDULE_LEVEL_WDAY)
		return 7;
	else
		return -1;
}

int Schedule::GetScheduleOffset(int level)
{
	if(level==SCHEDULE_LEVEL_SEC)
		return 0;
	else if(level==SCHEDULE_LEVEL_MIN)
		return 60;
	else if(level==SCHEDULE_LEVEL_HOUR)
		return 120;
	else if(level==SCHEDULE_LEVEL_DAY)
		return 144;
	else if(level==SCHEDULE_LEVEL_MONTH)
		return 175;
	else if(level==SCHEDULE_LEVEL_WDAY)
		return 187;
	else
		return -1;
}

time_t Schedule::GetNextTime(void)
{
	time_t now;
	time(&now);
	
	time_t next_time = get_next_time(now+1); // Do not start sooner as next second
	if(next_time<=now)
		return now+86400*365; // Prevent from starting in the past
	
	return next_time;
}

time_t Schedule::get_next_time(time_t now)
{
	time_t first;
	struct tm now_t;
	struct tm first_t;
	int time_level_now[SCHEDULE_LEVEL_MONTH+1];
	int time_level_first[SCHEDULE_LEVEL_MONTH+1];
	
	// Extract current time
	localtime_r(&now,&now_t);
	time_level_now[SCHEDULE_LEVEL_SEC] = now_t.tm_sec;
	time_level_now[SCHEDULE_LEVEL_MIN] = now_t.tm_min;
	time_level_now[SCHEDULE_LEVEL_HOUR] = now_t.tm_hour;
	time_level_now[SCHEDULE_LEVEL_DAY] = now_t.tm_mday-1;
	time_level_now[SCHEDULE_LEVEL_MONTH] = now_t.tm_mon;
	
	// Find first possible time in each level (seconds, minutes, hours, days, months)
	int next_level_offset = 0, current_level_offset = 0;
	for(int level=0;level<=SCHEDULE_LEVEL_MONTH;level++)
	{
		// Get new level properties
		int offset = GetScheduleOffset(level);
		int length = GetScheduleLength(level);
		
		// Reset reminder
		current_level_offset = next_level_offset;
		next_level_offset = 0;
		
		// Find if all subsequent levels are possible
		bool next_levels_valid = true;
		for(int k=level+1;k<=SCHEDULE_LEVEL_MONTH;k++)
		{
			if(!schedule[GetScheduleOffset(k)+time_level_now[k]])
			{
				next_levels_valid = false;
				break;
			}
		}
		
		int i,j;
		for(i=0;i<length;i++)
		{
			if(next_levels_valid)
				j = (time_level_now[level]+i+current_level_offset)%length; // If all subsequent levels are valid, we can start from now
			else
				j = i; // We have to start as soon as possible
			
			if(schedule[offset+j]) // check if schedule is possible at the current level
			{
				time_level_first[level] = j;
				if(next_levels_valid && j<time_level_now[level]+current_level_offset)
					next_level_offset = 1; // Nothing has been found between now and the end of the level, so hold a reminder (we will start +1 in the next level)
				break;
			}
		}
	}
	
	memset(&first_t,0,sizeof(struct tm));
	first_t.tm_sec = time_level_first[SCHEDULE_LEVEL_SEC];
	first_t.tm_min = time_level_first[SCHEDULE_LEVEL_MIN];
	first_t.tm_hour = time_level_first[SCHEDULE_LEVEL_HOUR];
	first_t.tm_mday = time_level_first[SCHEDULE_LEVEL_DAY]+1;
	first_t.tm_mon = time_level_first[SCHEDULE_LEVEL_MONTH];
	first_t.tm_year = now_t.tm_year+next_level_offset;
	first_t.tm_isdst = -1;
	
	first = mktime(&first_t);
	
	if(first_t.tm_mday!=time_level_first[SCHEDULE_LEVEL_DAY]+1 || first_t.tm_mon!=time_level_first[SCHEDULE_LEVEL_MONTH])
	{
		// Date is invalid, try next possible time
		return get_next_time(first);
	}
	
	if(!schedule[GetScheduleOffset(SCHEDULE_LEVEL_WDAY)+first_t.tm_wday])
	{
		// We found a matching day but week of day is wrong, try from next day
		first_t.tm_min = 0;
		first_t.tm_hour = 0;
		first_t.tm_mday++;
		first_t.tm_isdst = -1;
		first = mktime(&first_t);
		return get_next_time(first);
	}
	
	// Found it !
	return first;
}
