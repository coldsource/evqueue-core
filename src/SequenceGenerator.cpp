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

#include <SequenceGenerator.h>
#include <Configuration.h>
#include <DB.h>

using namespace std;

SequenceGenerator *SequenceGenerator::instance = 0;

SequenceGenerator::SequenceGenerator()
{
	instance = this;
	
	DB db;
	db.Query("SELECT MAX(workflow_instance_id) FROM t_workflow_instance");
	db.FetchRow();
	seq = db.GetFieldInt(0)+1;
}

SequenceGenerator::~SequenceGenerator()
{
	;
}

unsigned int SequenceGenerator::GetInc()
{
	unsigned int cur_seq;
	
	unique_lock<mutex> llock(lock);
	return seq++;
}