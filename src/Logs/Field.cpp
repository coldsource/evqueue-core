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

#include <Logs/Field.h>
#include <Exception/Exception.h>
#include <DB/DB.h>

using namespace std;

Field::Field()
{
	id = -1;
	type = NONE;
}

Field::Field(unsigned int id)
{
	DB db("elog");
	
	db.QueryPrintf("SELECT field_id, field_name, field_type FROM t_field WHERE field_id=%i", &id);
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
	
	throw Exception("Field","Unknown field type");
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
