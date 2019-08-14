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

#include <Configuration/Configuration.h>
#include <Exception/Exception.h>

#include <string.h>
#include <stdlib.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;

Configuration::Configuration(void)
{
}

Configuration::Configuration(const map<string,string> &entries)
{
	this->entries = entries;
}

Configuration::~Configuration(void)
{
}

bool Configuration::Set(const string &entry,const string &value)
{
	if(entries.count(entry)==0)
		return false;
	
	entries[entry] = value;
	return true;
}

const string &Configuration::Get(const string &entry) const
{
	map<string,string>::const_iterator it = entries.find(entry);
	if(it==entries.end())
		throw Exception("Configuration","Unknown configuration entry: "+entry);
	return it->second;
}

int Configuration::GetInt(const string &entry) const
{
	const string value = Get(entry);
	if(value.substr(0,2)=="0x")
		return strtol(value.c_str(),0,16);
	return strtol(value.c_str(),0,10);
}

int Configuration::GetSize(const string &entry) const
{
	const string value = Get(entry);
	if(value.substr(value.length()-1,1)=="K")
		return strtol(value.c_str(),0,10)*1024;
	else if(value.substr(value.length()-1,1)=="M")
		return strtol(value.c_str(),0,10)*1024*1024;
	else if(value.substr(value.length()-1,1)=="G")
		return strtol(value.c_str(),0,10)*1024*1024*1024;
	else
		return strtol(value.c_str(),0,10);
}

bool Configuration::GetBool(const string &entry) const
{
	const string value = Get(entry);
	if(value=="yes" || value=="true" || value=="1")
		return true;
	return false;
}

void Configuration::check_f_is_exec(const string &filename)
{
	uid_t uid = geteuid();
	gid_t gid = getegid();

	struct stat ste;
	if(stat(filename.c_str(),&ste)!=0)
		throw Exception("Configuration","File not found : "+filename);

	if(!S_ISREG(ste.st_mode))
		throw Exception("Configuration",filename+" is not a regular file");

	if(uid==ste.st_uid && (ste.st_mode & S_IXUSR))
		return;
	else if(gid==ste.st_gid && (ste.st_mode & S_IXGRP))
		return;
	else if(ste.st_mode & S_IXOTH)
		return;

	throw Exception("Configuration","File is not executable : "+filename);
}

void Configuration::check_d_is_writeable(const string &path)
{
	uid_t uid = geteuid();
	gid_t gid = getegid();

	struct stat ste;
	if(stat(path.c_str(),&ste)!=0)
		throw Exception("Configuration","Directory not found : "+path);

	if(!S_ISDIR(ste.st_mode))
		throw Exception("Configuration",path+" is not a directory");

	if(uid==ste.st_uid && (ste.st_mode & S_IWUSR))
		return;
	else if(gid==ste.st_gid && (ste.st_mode & S_IWGRP))
		return;
	else if(ste.st_mode & S_IWOTH)
		return;

	throw Exception("Configuration","Directory is not writeable : "+path);
}

void Configuration::check_bool_entry(const string &name)
{
	if(entries[name]=="yes" || entries[name]=="true" || entries[name]=="1")
		return;

	if(entries[name]=="no" || entries[name]=="false" || entries[name]=="0")
		return;

	throw Exception("Configuration",name+": invalid boolean value '"+entries[name]+"'");
}

void Configuration::check_int_entry(const string &name, bool signed_int)
{
	try
	{
		size_t l;
		int val = stoi(entries[name],&l);
		if(l!=entries[name].length())
			throw 1;

		if(!signed_int && val<0)
			throw 1;
	}
	catch(...)
	{
		throw Exception("Configuration", name+": invalid integer value '"+entries[name]+"'");
	}
}

void Configuration::check_size_entry(const string &name)
{
	try
	{
		size_t l;
		stoi(entries[name],&l);
		if(l==entries[name].length())
			return;

		string unit = entries[name].substr(l);
		if(unit!="K" && unit!="M" && unit!="G")
			throw 1;
	}
	catch(...)
	{
		throw Exception("Configuration", name+": invalid size value '"+entries[name]+"'");
	}
}
