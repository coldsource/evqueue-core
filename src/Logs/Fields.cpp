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
#include <global.h>

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
	
	if(j.type()!=nlohmann::json::value_t::object)
		throw Exception("Fields", "Incorrect fields configuration json");
	
	// Modify existing fields
	for(auto it = j.begin(); it!=j.end(); ++it)
	{
		string field_name = it.key();
		if(!CheckFieldName(field_name))
			throw Exception("ChannelGroup","Invalid field name : "+it.key(),"INVALID_PARAMETER");
		
		auto field = it.value();
		
		if(field.type()!=nlohmann::json::value_t::object)
			throw Exception("Fields", "Incorrect fields configuration json");
		
		if(!field.contains("type"))
			throw Exception("Fields", "Field type is mandatory", "INVALID_PARAMETER");
		
		if(field["type"].type()!=nlohmann::json::value_t::string)
			throw Exception("Fields", "Field type must be a string", "INVALID_PARAMETER");
		
		string type_str = field["type"];
		Field::en_type type = Field::StringToFieldType(type_str);
		
		if(!field.contains("id"))
			continue;
		
		if(field["id"].type()!=nlohmann::json::value_t::number_unsigned)
			throw Exception("Fields", "Field ID must be an integer", "INVALID_PARAMETER");
		
		unsigned int field_id = field["id"];
		if(fields.find(field_id)==fields.end())
			throw Exception("Fields", "Unknow field ID : "+to_string(field_id));
		
		if(fields[field_id].GetName()!=field_name)
			db.QueryPrintf("UPDATE t_field SET field_name=%s WHERE field_id=%i", &field_name, &field_id);
		
		if(fields[field_id].GetType()!=type)
			db.QueryPrintf("UPDATE t_field SET field_type=%s WHERE field_id=%i", &type_str, &field_id);
		
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
		
		string name = it.key();
		string type = field["type"];
		Field::StringToFieldType(type);
		
		db.QueryPrintf("INSERT INTO t_field(%c, field_name, field_type) VALUES(%i, %s, %s)", &col_name, &id, &name, &type);
	}
	
	db.CommitTransaction();
}

bool Fields::CheckFieldName(const string &field_name)
{
	int i,len;
	
	len = field_name.length();
	if(len==0 || len>CHANNEL_FIELD_NAME_MAX_LEN)
		return false;
	
	for(i=0;i<len;i++)
		if(!isalnum(field_name[i]))
			return false;
	
	if(field_name=="date" || field_name=="crit") // Reserved names
		return false;
	
	return true;
}
