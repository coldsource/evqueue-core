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

#include <ELogs/Field.h>
#include <ELogs/LogStorage.h>
#include <Exception/Exception.h>
#include <DB/DB.h>

#include <arpa/inet.h>

using namespace std;

namespace ELogs
{

Field::Field()
{
	id = -1;
	type = NONE;
}

Field::Field(unsigned int id)
{
	DB db("elog");
	
	db.QueryPrintf("SELECT field_id, field_name, field_type FROM t_field WHERE field_id=%i", {&id});
	if(!db.FetchRow())
		throw Exception("Field","Unknown field id "+to_string(id));
	
	this->id = db.GetFieldInt(0);
	name = db.GetField(1);
	type = StringToFieldType(db.GetField(2));
}

Field::Field(const Field &f)
{
	id = f.id;
	name = f.name;
	type = f.type;
}

Field::en_type Field::StringToFieldType(const string &str)
{
	if(str=="CHAR")
		return CHAR;
	else if(str=="INT")
		return INT;
	else if(str=="IP")
		return IP;
	else if(str=="PACK")
		return PACK;
	else if(str=="TEXT")
		return TEXT;
	else if(str=="ITEXT")
		return ITEXT;
	
	throw Exception("Field","Unknown field type : "+str);
}

string Field::FieldTypeToString(en_type type)
{
	if(type==CHAR)
		return "CHAR";
	else if(type==INT)
		return "INT";
	else if(type==IP)
		return "IP";
	else if(type==PACK)
		return "PACK";
	else if(type==TEXT)
		return "TEXT";
	else if(type==ITEXT)
		return "ITEXT";
	
	throw Exception("Field","Unknown field type");
}

void *Field::Pack(const string &str, int *val_int, string *val_str) const
{
	switch(type)
	{
		case Field::en_type::CHAR:
			*val_str = str;
			return val_str;
		
		case Field::en_type::TEXT:
			*val_str = str;
			return val_str;
		
		case Field::en_type::ITEXT:
			*val_str = str;
			return val_str;
		
		case Field::en_type::INT:
			*val_int = PackInteger(str);
			return val_int;
		
		case Field::en_type::IP:
			*val_str = PackIP(str);
			return val_str;
		
		case Field::en_type::PACK:
			*val_int = PackString(str);
			return val_int;
		
		case Field::en_type::NONE:
			return 0;
	}
	
	return 0; // Juste make compiler shuts
}

string Field::Unpack(const string &val) const
{
	if(val=="")
		return "";
	
	switch(type)
	{
		case Field::en_type::CHAR:
			return val;
		
		case Field::en_type::TEXT:
			return val;
		
		case Field::en_type::ITEXT:
			return val;
		
		case Field::en_type::INT:
			return UnpackInteger(stoi(val));
		
		case Field::en_type::IP:
			return UnpackIP(val);
		
		case Field::en_type::PACK:
			return UnpackString(stoi(val));
		
		case Field::en_type::NONE:
			return "";
	}
	
	return ""; // Juste make compiler shuts
}

int Field::PackCrit(const string &str)
{
	if(str=="LOG_EMERG")
		return 0;
	else if(str=="LOG_ALERT")
		return 1;
	else if(str=="LOG_CRIT")
		return 2;
	else if(str=="LOG_ERR")
		return 3;
	else if(str=="LOG_WARNING")
		return 4;
	else if(str=="LOG_NOTICE")
		return 5;
	else if(str=="LOG_INFO")
		return 6;
	else if(str=="LOG_DEBUG")
		return 7;
	
	throw Exception("Field", "Invalid criticality : "+str, "INVALID_PARAMETER");
}

string Field::UnpackCrit(int i)
{
	switch(i)
	{
		case 0:
			return "LOG_EMERG";
		case 1:
			return "LOG_ALERT";
		case 2:
			return "LOG_CRIT";
		case 3:
			return "LOG_ERR";
		case 4:
			return "LOG_WARNING";
		case 5:
			return "LOG_NOTICE";
		case 6:
			return "LOG_INFO";
		case 7:
			return "LOG_DEBUG";
	}
	
	throw Exception("Field", "Invalid integer criticality : "+to_string(i), "INVALID_PARAMETER");
}

int Field::PackInteger(const string &str) const
{
	try
	{
		return stoi(str);
	}
	catch(...)
	{
		throw Exception("Field", "Invalid integer : "+str);
	}
}

string Field::UnpackInteger(int i) const
{
	return to_string(i);
}

string Field::PackIP(const string &ip) const
{
	char bin[16];
	
	if(inet_pton(AF_INET, ip.c_str(), bin))
		return string(bin, 4);
	
	if(inet_pton(AF_INET6, ip.c_str(), bin))
		return string(bin, 16);
	
	throw Exception("Field", "Invalid IP : "+ip);
}

string Field::UnpackIP(const string &bin_ip) const
{
	char buf[64];
	
	if(bin_ip.size()==4)
	{
		if(!inet_ntop(AF_INET, bin_ip.c_str(), buf, 64))
			return "";
		
		return buf;
	}
	
	if(bin_ip.size()==16)
	{
		if(!inet_ntop(AF_INET6, bin_ip.c_str(), buf, 64))
			return "";
		
		return buf;
	}
	
	throw Exception("Field", "Invalid binary IP length should be 4 ou 16");
}

unsigned int Field::PackString(const string &str) const
{
	return LogStorage::GetInstance()->PackString(str);
}

string Field::UnpackString(int i) const
{
	return LogStorage::GetInstance()->UnpackString(i);
}

const string Field::GetTableName() const
{
	switch(type)
	{
		case Field::en_type::CHAR:
			return "t_value_char";
		
		case Field::en_type::TEXT:
			return "t_value_itext";
		
		case Field::en_type::ITEXT:
			return "t_value_itext";
		
		case Field::en_type::INT:
			return "t_value_int";
		
		case Field::en_type::IP:
			return "t_value_ip";
		
		case Field::en_type::PACK:
			return "t_value_pack";
		
		case Field::en_type::NONE:
			return "";
	}
	
	return ""; // Make compiler shuts
}

const string Field::GetDBType() const
{
	switch(type)
	{
		case Field::en_type::CHAR:
			return "%s";
		
		case Field::en_type::TEXT:
			return "%s";
		
		case Field::en_type::ITEXT:
			return "%s";
		
		case Field::en_type::INT:
			return "%i";
		
		case Field::en_type::IP:
			return "%s";
		
		case Field::en_type::PACK:
			return "%i";
		
		case Field::en_type::NONE:
			return "";
	}
	
	return ""; // Make compiler shuts
}

}
