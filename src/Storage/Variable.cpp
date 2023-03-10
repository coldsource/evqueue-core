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

#include <Storage/Variable.h>
#include <DB/DB.h>
#include <Exception/Exception.h>

using namespace std;
using nlohmann::json;

using namespace std;

namespace Storage
{

Variable::Variable(unsigned int id)
{
	DB db;
	
	db.QueryPrintf("SELECT storage_id, storage_path, storage_type, storage_structure, storage_name, storage_value FROM t_storage WHERE storage_id=%i", {&id});
	
	if(!db.FetchRow())
		throw Exception("Storage", "Variable not found : « " + to_string(id) + " »");
	
	init(db);
}

Variable::Variable(const string &path, const string &name)
{
	DB db;
	
	db.QueryPrintf("SELECT storage_id, storage_path, storage_type, storage_structure, storage_name, storage_value FROM t_storage WHERE storage_path=%s AND storage_name=%s", {&path, &name});
	
	if(!db.FetchRow())
		throw Exception("Storage", "Variable not found : « " + path + "/" + name + " »");
	
	init(db);
}

void Variable::init(const DB &db)
{
	id = db.GetFieldInt(0);
	path = db.GetField(1);
	type = db.GetField(2);
	structure = db.GetField(3);
	name = db.GetField(4);
	value = db.GetField(5);
	jvalue = json::parse(value);
}

}
