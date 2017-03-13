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

#include <RetrySchedule.h>
#include <RetrySchedules.h>
#include <Exception.h>
#include <DB.h>
#include <SocketQuerySAX2Handler.h>
#include <QueryResponse.h>
#include <XMLUtils.h>
#include <User.h>
#include <base64.h>
#include <global.h>

#include <string.h>

using namespace std;

extern string retry_schedule_xsd_str;

RetrySchedule::RetrySchedule(DB *db,const string &schedule_name)
{
	this->name = schedule_name;
	
	db->QueryPrintf("SELECT schedule_id, schedule_xml FROM t_schedule WHERE schedule_name=%s",&schedule_name);
	
	if(!db->FetchRow())
		throw Exception("RetrySchedule","Unknown retry schedule");
	
	id = db->GetFieldInt(0);
	schedule_xml = db->GetField(1);
}

bool RetrySchedule::CheckRetryScheduleName(const string &retry_schedule_name)
{
	int i,len;
	
	len = retry_schedule_name.length();
	if(len==0 || len>RETRY_SCHEDULE_NAME_MAX_LEN)
		return false;
	
	for(i=0;i<len;i++)
		if(!isalnum(retry_schedule_name[i]) && retry_schedule_name[i]!='_' && retry_schedule_name[i]!='-')
			return false;
	
	return true;
}

void RetrySchedule::Get(unsigned int id, QueryResponse *response)
{
	RetrySchedule retry_schedule = RetrySchedules::GetInstance()->Get(id);
	
	DOMElement node = (DOMElement)response->AppendXML(retry_schedule.GetXML());
	node.setAttribute("id",to_string(retry_schedule.GetID()));
	node.setAttribute("name",retry_schedule.GetName());
}

void RetrySchedule::Create(const std::string &name, const std::string &base64)
{
	string xml = create_edit_check(name,base64);
	
	DB db;
	db.QueryPrintf("INSERT INTO t_schedule(schedule_name, schedule_xml) VALUES(%s,%s)",&name,&xml);
}

void RetrySchedule::Edit(unsigned int id,const std::string &name, const std::string &base64)
{
	if(!RetrySchedules::GetInstance()->Exists(id))
		throw Exception("RetrySchedule","Retry schedule not found");
	
	string xml = create_edit_check(name,base64);
	
	DB db;
	db.QueryPrintf("UPDATE t_schedule SET schedule_name=%s, schedule_xml=%s WHERE schedule_id=%i",&name,&xml,&id);
}

void RetrySchedule::Delete(unsigned int id)
{
	DB db;
	db.QueryPrintf("DELETE FROM t_schedule WHERE schedule_id=%i",&id);
}

std::string RetrySchedule::create_edit_check(const std::string &name, const std::string &base64)
{
	if(!CheckRetryScheduleName(name))
		throw Exception("RetrySchedule","Invalid retry schedule name");
	
	string retry_schedule_xml;
	if(!base64_decode_string(retry_schedule_xml,base64))
		throw Exception("RetrySchedule","Invalid base64 sequence");
	
	XMLUtils::ValidateXML(retry_schedule_xml,retry_schedule_xsd_str);
	
	return retry_schedule_xml;
}

bool RetrySchedule::HandleQuery(const User &user, SocketQuerySAX2Handler *saxh, QueryResponse *response)
{
	if(!user.IsAdmin())
		User::InsufficientRights();
	
	const string action = saxh->GetRootAttribute("action");
	
	if(action=="get")
	{
		unsigned int id = saxh->GetRootAttributeInt("id");
		
		Get(id,response);
		
		return true;
	}
	else if(action=="create" || action=="edit")
	{
		string name = saxh->GetRootAttribute("name");
		string content = saxh->GetRootAttribute("content");
		
		if(action=="create")
			Create(name, content);
		else
		{
			unsigned int id = saxh->GetRootAttributeInt("id");
			
			Edit(id,name, content);
		}
		
		RetrySchedules::GetInstance()->Reload();
		
		return true;
	}
	else if(action=="delete")
	{
		unsigned int id = saxh->GetRootAttributeInt("id");
		
		Delete(id);
		
		RetrySchedules::GetInstance()->Reload();
		
		return true;
	}
	
	return false;
}