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

#include <Storage/Storage.h>
#include <API/QueryHandlers.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <User/User.h>
#include <DB/DB.h>
#include <Exception/Exception.h>
#include <WS/Events.h>

#include <regex>

using namespace std;
using nlohmann::json;

namespace Storage
{

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	qh->RegisterHandler("storage", Storage::HandleQuery);
	qh->RegisterModule("storage", EVQUEUE_VERSION);
	
	Events::GetInstance()->RegisterEvents({"VARIABLE_SET", "VARIABLE_UNSET"});
	return (APIAutoInit *)0;
});

void Storage::check_path(const std::string &path)
{
	if(path=="")
		throw Exception("Storage", "Path cannot be empty", "INVALID_PARAMETER");
	
	if(path.size()>STORAGE_PATH_MAXLEN)
		throw Exception("Storage", "Path is too long", "INVALID_PARAMETER");
	
	if(path[0]=='/' || path[path.size() - 1]=='/')
		throw Exception("Storage", "Path cannot start or end with /", "INVALID_PARAMETER");
	
	for(int i=0;i<path.size()-1;i++)
		if(path[i]=='/' && path[i+1]=='/')
			throw Exception("Storage", "Path cannot contain //", "INVALID_PARAMETER");
}

void Storage::check_name(const std::string &name)
{
	if(name=="")
		throw Exception("Storage", "Name cannot be empty", "INVALID_PARAMETER");
	
	if(name.size()>STORAGE_NAME_MAXLEN)
		throw Exception("Storage", "Name is too long", "INVALID_PARAMETER");
}

void Storage::check_value(const std::string &value, const std::string type, const std::string structure)
{
	if(value.size()>STORAGE_VAR_MAXLEN)
		throw Exception("Storage", "Variable value is too long", "INVALID_PARAMETER");
	
	json j;
	try
	{
		j = json::parse(value);
	}
	catch(...)
	{
		throw Exception("Storage", "Invalid json", "INVALID_PARAMETER");
	}
	
	// First check structure
	if(structure=="ARRAY")
	{
		if(j.type()!=json::value_t::array)
			throw Exception("Storage", "Invalid json : expecting Array based on given structure", "INVALID_PARAMETER");
		
		for(int i = 0; i<j.size(); i++)
			check_value_type(j[i], type);
	}
	else if(structure=="MAP")
	{
		if(j.type()!=json::value_t::object)
			throw Exception("Storage", "Invalid json : expecting Object based on given structure", "INVALID_PARAMETER");
		
		for(auto it = j.begin(); it!=j.end(); ++it)
			check_value_type(it.value(), type);
	}
	else if(structure=="NONE")
		check_value_type(j, type);
}

void Storage::check_value_type(const json &j, const string &type)
{
	if(type=="INT")
	{
		if(j.type()!=json::value_t::number_integer && j.type()!=json::value_t::number_unsigned)
			throw Exception("Storage", "Invalid json : expecting Integer based on given type for « " + j.dump() + " »", "INVALID_PARAMETER");
	}
	else if(type=="STRING")
	{
		if(j.type()!=json::value_t::string)
			throw Exception("Storage", "Invalid json : expecting String based on given type for « " + j.dump() + " »", "INVALID_PARAMETER");
	}
	else if(type=="BOOLEAN")
	{
		if(j.type()!=json::value_t::boolean)
			throw Exception("Storage", "Invalid json : expecting Boolean based on given type for « " + j.dump() + " »", "INVALID_PARAMETER");
	}
	else
		throw Exception("Storage", "Invalid type : « " + type + " »", "INVALID_PARAMETER");
}

unsigned int Storage::Set(unsigned int id, const string &path, const string &name, const string &type, const string &structure, const string &value)
{
	check_path(path);
	check_name(name);
	check_value(value, type, structure);
	
	DB db;
	if(id==0)
	{
		db.QueryPrintf(
			"REPLACE INTO t_storage(storage_path, storage_type, storage_structure, storage_name, storage_value) VALUES(%s, %s, %s, %s, %s)",
			{&path, &type, &structure, &name, &value}
		);
	}
	else
	{
		db.QueryPrintf(
			"UPDATE t_storage SET storage_path=%s, storage_type=%s, storage_structure=%s, storage_name=%s, storage_value=%s WHERE storage_id=%i",
			{&path, &type, &structure, &name, &value, &id}
		);
	}
	
	Events::GetInstance()->Create("VARIABLE_SET");
	
	return 0;
}

unsigned int Storage::Unset(unsigned int id, const string &path, const string &name)
{
	DB db;
	if(id==0)
		db.QueryPrintf("DELETE FROM t_storage WHERE storage_path=%s AND storage_name=%s", {&path, &name});
	else
		db.QueryPrintf("DELETE FROM t_storage WHERE storage_id=%i", {&id});
	
	Events::GetInstance()->Create("VARIABLE_UNSET");
	
	return 0;
}

void Storage::Get(unsigned int id, const string &path, const string &name, QueryResponse *response)
{
	DB db;
	
	if(id==0)
		db.QueryPrintf("SELECT storage_path, storage_type, storage_structure, storage_name, storage_value FROM t_storage WHERE storage_path=%s AND storage_name=%s", {&path, &name});
	else
		db.QueryPrintf("SELECT storage_path, storage_type, storage_structure, storage_name, storage_value FROM t_storage WHERE storage_id=%i", {&id});
	
	if(!db.FetchRow())
	{
		if(id!=0)
			throw Exception("Storage", "Variable not found : « " + to_string(id) + " »");
		else
			throw Exception("Storage", "Variable not found : « " + path + "/" + name + " »");
	}
	
	response->SetAttribute("path", db.GetField(0));
	response->SetAttribute("type", db.GetField(1));
	response->SetAttribute("structure", db.GetField(2));
	response->SetAttribute("name", db.GetField(3));
	response->SetAttribute("value", db.GetField(4));
}

void Storage::List(const std::string &path, bool recursive, QueryResponse *response)
{
	DB db;
	
	if(!recursive)
		db.QueryPrintf("SELECT storage_name, storage_value FROM t_storage WHERE storage_path=%s", {&path});
	else
	{
		if(path=="")
			db.Query("SELECT storage_id,storage_name, storage_value, storage_path FROM t_storage");
		else
			db.QueryPrintf("SELECT storage_id, storage_name, storage_value, storage_path FROM t_storage WHERE (storage_path LIKE CONCAT(%s, '/%%') OR storage_path=%s)", {&path, &path});
	}
	
	
	while(db.FetchRow())
	{
		DOMElement node_var = (DOMElement)response->AppendXML("<var />");
		node_var.setAttribute("id", db.GetField(0));
		node_var.setAttribute("name", db.GetField(1));
		node_var.setAttribute("value", db.GetField(2));
		if(recursive)
			node_var.setAttribute("path", db.GetField(3));
			
	}
}

bool Storage::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	string action = query->GetRootAttribute("action");
	
	if(action=="set")
	{
		unsigned int id = query->GetRootAttributeInt("id", 0);
		string path = query->GetRootAttribute("path", "");
		string name = query->GetRootAttribute("name", "");
		string type = query->GetRootAttribute("type");
		string structure = query->GetRootAttribute("structure");
		string value = query->GetRootAttribute("value");
		
		if(id==0 && path=="" && name=="")
			throw Exception("Storage", "You must provied either « id » or « path » AND « name »", "MISSING_PARAMETER");
		
		Set(id, path, name, type, structure, value);
		
		return true;
	}
	else if(action=="unset")
	{
		unsigned int id = query->GetRootAttributeInt("id", 0);
		string path = query->GetRootAttribute("path", "");
		string name = query->GetRootAttribute("name", "");
		
		if(id==0 && path=="" && name=="")
			throw Exception("Storage", "You must provied either « id » or « path » AND « name »", "MISSING_PARAMETER");
		
		Unset(id, path, name);
		
		return true;
	}
	else if(action=="get")
	{
		unsigned int id = query->GetRootAttributeInt("id", 0);
		string path = query->GetRootAttribute("path", "");
		string name = query->GetRootAttribute("name", "");
		
		if(id==0 && name=="")
		{
			// Only path was provided, try to extract name from it
			regex split_regex ("([^/]+)/([^/]+)$");
			smatch match;
			
			if(regex_match(path, match, split_regex) && match.size()==3)
			{
				path = match[1].str();
				name = match[2].str();
			}
			else
				throw Exception("Storage", "When name is empty, path must be a absolute variable name, including path and name", "INVALID_PARAMETER");
		}
		
		if(id==0 && path=="" && name=="")
			throw Exception("Storage", "You must provied either « id », « path » and/or « name »", "MISSING_PARAMETER");
		
		Get(id, path, name, response);
		
		return true;
	}
	else if(action=="list")
	{
		string path = query->GetRootAttribute("path");
		bool recursive = query->GetRootAttributeBool("recursive", false);
		
		List(path, recursive, response);
		
		return true;
	}
	
	return false;
}

}
