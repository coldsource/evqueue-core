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

#include <Storage/Launchers.h>
#include <Storage/Launcher.h>
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

Launchers *Launchers::instance = 0;

static auto init = QueryHandlers::GetInstance()->RegisterInit([](QueryHandlers *qh) {
	qh->RegisterHandler("launchers", Launchers::HandleQuery);
	qh->RegisterReloadHandler("launchers", Launchers::HandleReload);
	return (APIAutoInit *)new Launchers();
});

Launchers::Launchers():APIObjectList("Launcher")
{
	instance = this;
	
	Reload(false);
}

Launchers::~Launchers()
{
}

void Launchers::List(QueryResponse *response, const User &user)
{
	Launchers *launchers = Launchers::GetInstance();
	unique_lock<mutex> llock(launchers->lock);
	
	for(auto it = launchers->objects_name.begin(); it!=launchers->objects_name.end(); it++)
	{
		Launcher launcher= *it->second;
	
		if(!user.HasAccessToModule("storage", "launcher", launcher.GetID()))
			continue;
		
		DOMElement node_var = (DOMElement)response->AppendXML("<launcher />");
		node_var.setAttribute("id", to_string(launcher.GetID()));
		node_var.setAttribute("name", launcher.GetName());
		node_var.setAttribute("group", launcher.GetGroup());
		node_var.setAttribute("description", launcher.GetDescription());
	}
}

void Launchers::Reload(bool notify)
{
	Logger::Log(LOG_NOTICE,"Reloading launchers definitions");
	
	unique_lock<mutex> llock(lock);
	
	clear();
	
	// Update
	DB db;
	DB db2(&db);
	db.Query("SELECT launcher_id, launcher_name FROM t_launcher");
	
	while(db.FetchRow())
		add(db.GetFieldInt(0), db.GetField(1), new Launcher(&db2,db.GetFieldInt(0)));
	
	llock.unlock();
	
	if(notify)
	{
		// Notify cluster
		Cluster::GetInstance()->Notify("<control action='reload' module='launchers' notify='no' />\n");
	}
}

bool Launchers::HandleQuery(const User &user, XMLQuery *query, QueryResponse *response)
{
	string action = query->GetRootAttribute("action");
	
	if(action=="list")
	{
		List(response, user);
		
		return true;
	}
	
	return false;
}

void Launchers::HandleReload(bool notify)
{
	Launchers *launchers = Launchers::GetInstance();
	launchers->Reload(notify);
}

}
