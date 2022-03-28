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

#include <Logs/Fields.h>
#include <Exception/Exception.h>
#include <DB/DB.h>

#include <set>

using namespace std;
using nlohmann::json;

Fields::Fields(en_type type, unsigned int id)
{
	DB db("elog");
	
	this->type = type;
	this->id = id;
	
	if(type==GROUP)
		col_name = "channel_group_id";
	else
		col_name = "channel_id";
	
	db.QueryPrintf("SELECT field_id FROM t_field WHERE %c=%i", &col_name, &id);
	
	while(db.FetchRow())
		fields[db.GetFieldInt(0)] = Field(db.GetFieldInt(0));
}

void Fields::Update(const json &j)
{
	DB db("elog");
	
	db.StartTransaction();
	
	set<unsigned int> fields_ids;
	
	// Rename existing fields
	for(auto it = j.begin(); it!=j.end(); ++it)
	{
		auto field = it.value();
		
		if(!field.contains("name"))
			throw Exception("Fields", "Field name is mandatory");
		
		if(field["name"].type()!=nlohmann::json::value_t::string)
			throw Exception("Fields", "Field name must be a string");
		
		if(!field.contains("id"))
			continue;
		
		if(field["id"].type()!=nlohmann::json::value_t::number_unsigned)
			throw Exception("Fields", "Field ID must be an integer");
		
		unsigned int field_id = field["id"];
		if(fields.find(field_id)==fields.end())
			throw Exception("Fields", "Unknow field ID : "+to_string(field_id));
		
		string field_name = field["name"];
		if(fields[field_id].GetName()!=field_name)
			db.QueryPrintf("UPDATE t_field SET field_name=%s WHERE %c=%i", &field_name, &col_name, &field_id);
		
		fields_ids.insert(field_id);
	}
	
	// Delete fields that are no longer in config
	for(auto it = fields.begin(); it!=fields.end(); ++it)
	{
		unsigned int field_id = it->first;
		
		if(fields_ids.count(field_id)==0)
			db.QueryPrintf("DELETE FROM t_field WHERE field_id=%i", &field_id);
	}
	
	// Create new fields
	for(auto it = j.begin(); it!=j.end(); ++it)
	{
		auto field = it.value();
		if(field.contains("id"))
			continue;
		
		if(!field.contains("type"))
			throw Exception("Fields", "Field type is mandatory");
		
		if(field["type"].type()!=nlohmann::json::value_t::string)
			throw Exception("Fields", "Field type must be a string");
		
		string name = field["name"];
		string type = field["type"];
		Field::StringToFieldType(type);
		
		db.QueryPrintf("INSERT INTO t_field(%c, field_name, field_type) VALUES(%i, %s, %s)", &col_name, &id, &name, &type);
	}
	
	db.CommitTransaction();
}
