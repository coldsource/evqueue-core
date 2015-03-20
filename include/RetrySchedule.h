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

#ifndef _RETRYSCHEDULE_H_
#define _RETRYSCHEDULE_H_

class DB;

class RetrySchedule
{
	char *schedule_xml;
		
	public:
		RetrySchedule();
		RetrySchedule(DB *db,const char *schedule_name);
		RetrySchedule(const RetrySchedule &schedule);
		~RetrySchedule();
		
		RetrySchedule &operator=(const RetrySchedule &schedule);
		
		const char *GetXML() { return schedule_xml; }
	
	private:
		void free(void);
};

#endif
