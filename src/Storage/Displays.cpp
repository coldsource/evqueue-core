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

#include <Storage/Displays.h>
#include <Storage/Display.h>
#include <User/User.h>
#include <Exception/Exception.h>
#include <DB/DB.h>
#include <Logger/Logger.h>
#include <API/XMLQuery.h>
#include <API/QueryResponse.h>
#include <Cluster/Cluster.h>
#include <API/QueryHandlers.h>

using namespace std;

namespace Storage
{

Displays *Displays::instance = 0;

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	qh->RegisterHandler("displays", Displays::HandleQuery);
	qh->RegisterReloadHandler("displays", Displays::HandleReload);
	return (APIAutoInit *)new Displays();
});

Displays::Displays():APIObjectList("Display")
{
	instance = this;
	
	Reload(false);
}

Displays::~Displays()
{
}

void Displays::List(QueryResponse *response, const User &user)
{
	Displays *displays = Displays::GetInstance();
	unique_lock<mutex> llock(displays->lock);
	
	for(auto it = displays->objects_name.begin(); it!=displays->objects_name.end(); it++)
	{
		Display display= *it->second;
	
		DOMElement node_var = (DOMElement)response->AppendXML("<display />");
		node_var.setAttribute("id", to_string(display.GetID()));
		node_var.setAttribute("name", display.GetName());
	}
}

void Displays::Reload(bool notify)
{
	Logger::Log(LOG_NOTICE,"Reloading displays definitions");
	
	unique_lock<mutex> llock(lock);
	
	clear();
	
	// Update
	DB db;
	DB db2(&db);
	db.Query("SELECT display_id, display_name FROM t_display");
	
	while(db.FetchRow())
		add(db.GetFieldInt(0), db.GetField(1), new Display(&db2,db.GetFieldInt(0)));
	
	llock.unlock();
	
	if(notify)
	{
		// Notify cluster
		Cluster::GetInstance()->Notify("<control action='reload' module='displays' notify='no' />\n");
	}
}

bool Displays::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	string action = query->GetRootAttribute("action");
	
	if(action=="list")
	{
		List(response, user);
		
		return true;
	}
	
	return false;
}

void Displays::HandleReload(bool notify)
{
	Displays *displays = Displays::GetInstance();
	displays->Reload(notify);
}

}
