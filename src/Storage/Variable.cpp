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
#include <Storage/Storage.h>
#include <DB/DB.h>
#include <Exception/Exception.h>
#include <API/QueryHandlers.h>
#include <WS/Events.h>

using namespace std;
using nlohmann::json;

using namespace std;

namespace Storage
{

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	Events::GetInstance()->RegisterEvents({"VARIABLE_SET", "VARIABLE_UNSET"});
	return (APIAutoInit *)0;
});

Variable::Variable()
{
	id = 0;
}

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

void Variable::Update()
{
	// Be sure we have no empty values
	check_name(name);
	check_path(path);
	if(type=="")
		throw Exception("Variable", "Type cannot be empty", "INVALID_PARAMETER");
	if(structure=="")
		throw Exception("Variable", "Structure cannot be empty", "INVALID_PARAMETER");
	
	string value = jvalue.dump();
	
	DB db;
	if(id==0)
	{
		db.QueryPrintf(
			"REPLACE INTO t_storage(storage_path, storage_type, storage_structure, storage_name, storage_value) VALUES(%s, %s, %s, %s, %s)",
			{&path, &type, &structure, &name, &value}
		);
		
		id = db.InsertID();
	}
	else
	{
		db.QueryPrintf(
			"UPDATE t_storage SET storage_path=%s, storage_type=%s, storage_structure=%s, storage_name=%s, storage_value=%s WHERE storage_id=%i",
			{&path, &type, &structure, &name, &value, &id}
		);
	}
	
	Events::GetInstance()->Create("VARIABLE_SET", id);
}

void Variable::Unset()
{
	DB db;
	db.QueryPrintf("DELETE FROM t_storage WHERE storage_id=%i", {&id});
	
	Events::GetInstance()->Create("VARIABLE_UNSET", id);
	
	id = 0;
}

void Variable::init(const DB &db)
{
	id = db.GetFieldInt(0);
	path = db.GetField(1);
	type = db.GetField(2);
	structure = db.GetField(3);
	name = db.GetField(4);
	jvalue = json::parse(db.GetField(5));
}

json Variable::Head() const
{
	if(structure!="ARRAY")
		throw Exception("Variable", "Tail can only be used on array", "TYPE_ERROR");
	
	if(jvalue.size()==0)
		throw Exception("Variable", "Array is empty", "TYPE_ERROR");
	
	return jvalue[0];
}

json Variable::Tail() const
{
	if(structure!="ARRAY")
		throw Exception("Variable", "Tail can only be used on array", "TYPE_ERROR");
	
	if(jvalue.size()==0)
		throw Exception("Variable", "Array is empty", "TYPE_ERROR");
	
	return jvalue[jvalue.size()-1];
}

void Variable::check_path(const string &path) const
{
	if(path=="")
		throw Exception("Variable", "Path cannot be empty", "INVALID_PARAMETER");
	
	if(path.size()>STORAGE_PATH_MAXLEN)
		throw Exception("Variable", "Path is too long", "INVALID_PARAMETER");
	
	if(path[0]=='/' || path[path.size() - 1]=='/')
		throw Exception("Variable", "Path cannot start or end with /", "INVALID_PARAMETER");
	
	for(int i=0;i<path.size()-1;i++)
		if(path[i]=='/' && path[i+1]=='/')
			throw Exception("Variable", "Path cannot contain //", "INVALID_PARAMETER");
}

void Variable::check_name(const string &name) const
{
	if(name=="")
		throw Exception("Variable", "Name cannot be empty", "INVALID_PARAMETER");
	
	if(name.size()>STORAGE_NAME_MAXLEN)
		throw Exception("Variable", "Name is too long", "INVALID_PARAMETER");
	
	for(int i=0;i<name.size();i++)
	{
		if(name[i]=='/')
			throw Exception("Variable", "Variable name cannot contain /", "INVALID_PARAMETER");
	}
}

void Variable::check_value(const string &value) const
{
	if(type=="")
		throw Exception("Variable", "Type must be set before value", "INVALID_PARAMETER");
	
	if(structure=="")
		throw Exception("Variable", "Structure must be set before value", "INVALID_PARAMETER");
	
	if(value=="")
		throw Exception("Variable", "Value cannot be empty", "INVALID_PARAMETER");
	
	if(value.size()>STORAGE_VAR_MAXLEN)
		throw Exception("Variable", "Variable value is too long", "INVALID_PARAMETER");
	
	json j;
	try
	{
		j = json::parse(value);
	}
	catch(...)
	{
		throw Exception("Variable", "Invalid json", "INVALID_PARAMETER");
	}
	
	// First check structure
	if(structure=="ARRAY")
	{
		if(j.type()!=json::value_t::array)
			throw Exception("Variable", "Invalid json : expecting Array based on given structure", "INVALID_PARAMETER");
		
		for(int i = 0; i<j.size(); i++)
			check_value_type(j[i]);
	}
	else if(structure=="MAP")
	{
		if(j.type()!=json::value_t::object)
			throw Exception("Variable", "Invalid json : expecting Object based on given structure", "INVALID_PARAMETER");
		
		for(auto it = j.begin(); it!=j.end(); ++it)
			check_value_type(it.value());
	}
	else if(structure=="NONE")
		check_value_type(j);
}

void Variable::check_value_type(const json &j) const
{
	if(type=="")
		throw Exception("Variable", "Type must be set before value", "INVALID_PARAMETER");
	
	if(type=="INT")
	{
		if(j.type()!=json::value_t::number_integer && j.type()!=json::value_t::number_unsigned)
			throw Exception("Variable", "Invalid json : expecting Integer based on given type for « " + j.dump() + " »", "INVALID_PARAMETER");
	}
	else if(type=="STRING")
	{
		if(j.type()!=json::value_t::string)
			throw Exception("Variable", "Invalid json : expecting String based on given type for « " + j.dump() + " »", "INVALID_PARAMETER");
	}
	else if(type=="BOOLEAN")
	{
		if(j.type()!=json::value_t::boolean)
			throw Exception("Variable", "Invalid json : expecting Boolean based on given type for « " + j.dump() + " »", "INVALID_PARAMETER");
	}
	else
		throw Exception("Variable", "Invalid type : « " + type + " »", "INVALID_PARAMETER");
}

void Variable::SetPath(const string &path)
{
	check_path(path);
	
	this->path = path;
}

void Variable::SetType(const string &type)
{
	if(type!="INT" && type!="STRING" && type!="BOOLEAN")
		throw Exception("Variable", "Invalid type : « " + type +" »", "INVALID_PARAMETER");
	
	this->type = type;
}

void Variable::SetStructure(const string &structure)
{
	if(structure!="NONE" && structure!="ARRAY" && structure!="MAP")
		throw Exception("Variable", "Invalid structure : « " + structure +" »", "INVALID_PARAMETER");
	
	this->structure = structure;
}

void Variable::SetName(const string &name)
{
	check_name(name);
	
	this->name = name;
}

void Variable::SetValue(const string &value)
{
	check_value(value);
	
	this->jvalue = json::parse(value);
}

void Variable::SetValue(const json &jvalue)
{
	this->jvalue = jvalue;
}

void Variable::Append(const string &value)
{
	json jval;
	try
	{
		jval = json::parse(value);
	}
	catch(...)
	{
		throw Exception("Storage", "Invalid json", "INVALID_PARAMETER");
	}
	
	if(structure!="ARRAY")
		throw Exception("Storage", "Append can only be used on array", "TYPE_ERROR");
	
	check_value_type(jval);
	
	jvalue.push_back(jval);
}

void Variable::Append(const string &key, const string &value)
{
	json jval;
	try
	{
		jval = json::parse(value);
	}
	catch(...)
	{
		throw Exception("Storage", "Invalid json", "INVALID_PARAMETER");
	}
	
	if(structure!="MAP")
		throw Exception("Storage", "Append can only be used on map", "TYPE_ERROR");
	
	check_value_type(jval);
	
	jvalue[key] = jval;
}

void Variable::Add(int n)
{
	if(structure!="NONE")
		throw Exception("Storage", "Add can only be used on unstructured variables", "TYPE_ERROR");
	
	if(type!="INT")
		throw Exception("Storage", "Add can only be used on integer variables", "TYPE_ERROR");
	
	jvalue = (int)jvalue + n;
}

}
