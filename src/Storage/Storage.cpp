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

#include <regex>

using namespace std;
using nlohmann::json;

namespace Storage
{

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	qh->RegisterHandler("storage", Storage::HandleQuery);
	qh->RegisterModule("storage", EVQUEUE_VERSION);
	return (APIAutoInit *)0;
});

void Storage::split_path(const std::string filename, std::string &path, std::string &name)
{
	regex split_regex ("/([^/]+)$");
	smatch match;
	
	if(regex_search(filename, match, split_regex) && match.size()==2)
	{
		name = match[1].str();
		path = path.substr(0, path.size() - name.size() - 1);
	}
	else
		throw Exception("Storage", "When name is empty, path must be a absolute variable name, including path and name", "INVALID_PARAMETER");
}

Variable Storage::get_variable_from_query(XMLQuery *query, bool accept_create)
{
	unsigned int id = query->GetRootAttributeInt("id", 0);
	string path = query->GetRootAttribute("path", "");
	string name = query->GetRootAttribute("name", "");
	
	if(id!=0)
		return Variable(id);
	
	if(path!="" && name=="")
		split_path(path, path, name);
	
	try
	{
		if(path!="" && name!="")
			return Variable(path, name);
	}
	catch(Exception &e)
	{
		if(accept_create)
		{
			Variable v;
			v.SetPath(path);
			v.SetName(name);
			return v;
		}
		
		throw e;
	}
	
	throw Exception("Storage", "You must provied either « id », « path » and/or « name »", "MISSING_PARAMETER");
}

unsigned int Storage::Set(Variable &v, const string &path, const string &name, const string &type, const string &structure, const string &value)
{
	if(path!="" && name!="")
	{
		v.SetPath(path);
		v.SetName(name);
	}
	
	if(type!="")
		v.SetType(type);
	
	if(structure!="")
		v.SetStructure(structure);
	
	v.SetValue(value);
	
	v.Update();
	
	return 0;
}

void Storage::Append(Variable &v, const string &value)
{
	v.Append(value);
	v.Update();
}

void Storage::Append(Variable &v, const string &key, const string &value)
{
	v.Append(key, value);
	v.Update();
}

void Storage::Get(const Variable &v, QueryResponse *response)
{
	response->SetAttribute("id", to_string(v.GetID()));
	response->SetAttribute("path", v.GetPath());
	response->SetAttribute("type", v.GetType());
	response->SetAttribute("structure", v.GetStructure());
	response->SetAttribute("name", v.GetName());
	response->SetAttribute("value", v.GetValue());
}

void Storage::Head(const Variable &v, QueryResponse *response)
{
	response->SetAttribute("value", v.Head());
}

void Storage::Tail(const Variable &v, QueryResponse *response)
{
	response->SetAttribute("value", v.Tail());
}

void Storage::List(const std::string &path, bool recursive, QueryResponse *response)
{
	DB db;
	
	if(!recursive)
		db.QueryPrintf("SELECT storage_id, storage_name, storage_value, storage_path FROM t_storage WHERE storage_path=%s", {&path});
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
	string action = query->GetRootAttribute("action");
	
	if(action=="set")
	{
		if(!user.IsAdmin())
			User::InsufficientRights();
		
		Variable v = get_variable_from_query(query, true);
		string path = query->GetRootAttribute("path", "");
		string name = query->GetRootAttribute("name", "");
		string type = query->GetRootAttribute("type", "");
		string structure = query->GetRootAttribute("structure", "");
		string value = query->GetRootAttribute("value");
		
		Set(v, path, name, type, structure, value);
		
		return true;
	}
	else if(action=="append")
	{
		if(!user.IsAdmin())
			User::InsufficientRights();
		
		Variable v = get_variable_from_query(query);
		string key = query->GetRootAttribute("key", "");
		string value = query->GetRootAttribute("value");
		
		if(key!="")
			Append(v, key, value);
		else
			Append(v, value);
		
		return true;
	}
	else if(action=="inc" || action=="dec" || action=="add")
	{
		if(!user.IsAdmin())
			User::InsufficientRights();
		
		int n = 0;
		if(action=="inc")
			n = 1;
		else if(action=="dec")
			n = -1;
		else if(action=="add")
			n = query->GetRootAttributeInt("value");
		
		Variable v = get_variable_from_query(query);
		v.Add(n);
		v.Update();
		response->SetAttribute("value", v.GetValue());
		
		return true;
	}
	else if(action=="unset")
	{
		if(!user.IsAdmin())
			User::InsufficientRights();
		
		get_variable_from_query(query).Unset();
		return true;
	}
	else if(action=="get")
	{
		Get(get_variable_from_query(query), response);
		return true;
	}
	else if(action=="head")
	{
		Head(get_variable_from_query(query), response);
		return true;
	}
	else if(action=="tail")
	{
		Tail(get_variable_from_query(query), response);
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
