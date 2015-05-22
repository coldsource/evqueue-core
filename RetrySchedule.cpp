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

#include <RetrySchedule.h>
#include <Exception.h>
#include <DB.h>

#include <string.h>

using namespace std;

RetrySchedule::RetrySchedule(DB *db,const char *schedule_name)
{
	db->QueryPrintf("SELECT schedule_xml FROM t_schedule WHERE schedule_name=%s",schedule_name);
	
	if(!db->FetchRow())
		throw Exception("RetrySchedule","Unknown retry schedule");
	
	schedule_xml = db->GetField(0);
}