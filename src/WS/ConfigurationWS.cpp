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

#include <WS/ConfigurationWS.h>
#include <Exception/Exception.h>

#include <libwebsockets.h>

using namespace std;

static auto init = Configuration::GetInstance()->RegisterConfig(new ConfigurationWS());

ConfigurationWS::ConfigurationWS()
{
	entries["ws.bind.ip"] = "127.0.0.1";
	entries["ws.bind.port"] = "5001";
	entries["ws.connections.max"] = "128";
	entries["ws.listen.backlog"] = "64";
	entries["ws.rcv.timeout"] = "30";
	entries["ws.snd.timeout"] = "30";
	entries["ws.keepalive"] = "30";
	entries["ws.workers"] = "8";
	entries["ws.events.throttling"] = "yes";
	entries["ws.ssl.crt"] = "";
	entries["ws.ssl.key"] = "";
}

ConfigurationWS::~ConfigurationWS()
{
}

void ConfigurationWS::Check(void)
{
	check_bool_entry("ws.events.throttling");
	
	check_int_entry("ws.connections.max");
	check_int_entry("ws.listen.backlog");
	check_int_entry("ws.rcv.timeout");
	check_int_entry("ws.snd.timeout");
	check_int_entry("ws.keepalive");
	check_int_entry("ws.workers");
	check_int_entry("ws.bind.port");
	
	if(GetInt("ws.workers")<1)
		throw Exception("Configuration","ws.workers must be greater than 0");
	
	if((Get("ws.ssl.crt")!="" || Get("ws.ssl.key")!="") && (Get("ws.ssl.crt")=="" || Get("ws.ssl.key")==""))
		throw Exception("Configuration","To enable SSL, both ws.ssl.crt and ws.ssl.key must be set");
	
#if LWS_LIBRARY_VERSION_MAJOR < 3
	if(Get("ws.ssl.crt")!="" || Get("ws.ssl.key")!="")
		throw Exception("Configuration","SSL support is only available for libwebsockets >= 3.0");
#endif
}

