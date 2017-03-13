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

#include <Configuration.h>
#include <Exception.h>
#include <QueryResponse.h>
#include <DOMDocument.h>

#include <string.h>
#include <stdlib.h>
#include <stdlib.h>
#include <pcrecpp.h>

using namespace std;

Configuration *Configuration::instance=0;

Configuration::Configuration(void)
{
	// Load default configuration
	entries["core.ipc.qid"] = "0xEA023E3C";
	entries["core.gid"] = "0";
	entries["core.pidfile"] = "/var/run/evqueue/evqueue.pid";
	entries["core.uid"] = "0";
	entries["core.wd"] = "";
	entries["core.locale"] = "C.UTF-8";
	entries["core.auth.enable"] = "yes";
	entries["core.fastshutdown"] = "yes";
	entries["dpd.interval"] = "10";
	entries["queuepool.scheduler"] = "fifo";
	entries["gc.delay"] = "2";
	entries["gc.enable"] = "yes";
	entries["gc.interval"] = "43200";
	entries["gc.limit"] = "1000";
	entries["gc.logs.retention"] = "7";
	entries["gc.workflowinstance.retention"] = "30";
	entries["logger.db.enable"] = "yes";
	entries["logger.db.filter"] = "LOG_WARNING";
	entries["logger.syslog.enable"] = "yes";
	entries["logger.syslog.filter"] = "LOG_NOTICE";
	entries["mysql.database"] = "queueing";
	entries["mysql.host"] = "localhost";
	entries["mysql.password"] = "";
	entries["mysql.user"] = "";
	entries["network.bind.ip"] = "";
	entries["network.bind.path"] = "";
	entries["network.bind.port"] = "5000";
	entries["network.connections.max"] = "128";
	entries["network.listen.backlog"] = "64";
	entries["network.rcv.timeout"] = "30";
	entries["network.snd.timeout"] = "30";
	entries["notifications.monitor.path"] = "/usr/bin/evqueue_notification_monitor";
	entries["notifications.tasks.directory"] = "/usr/share/evqueue/plugins/notifications";
	entries["notifications.tasks.timeout"] = "5";
	entries["notifications.tasks.concurrency"] = "16";
	entries["processmanager.errlogs.directory"] = "errlogs";
	entries["processmanager.errlogs.enable"] = "no";
	entries["processmanager.logs.delete"] = "yes";
	entries["processmanager.logs.directory"] = "/tmp";
	entries["processmanager.monitor.path"] = "/usr/bin/evqueue_monitor";
	entries["processmanager.monitor.ssh_key"] = "";
	entries["processmanager.monitor.ssh_path"] = "/usr/bin/ssh";
	entries["processmanager.agent.path"] = "/usr/bin/evqueue_agent";
	entries["processmanager.tasks.directory"] = ".";
	entries["workflowinstance.saveparameters"] = "yes";
	entries["workflowinstance.savepoint.level"] = "2";
	entries["workflowinstance.savepoint.retry.enable"] = "yes";
	entries["workflowinstance.savepoint.retry.times"] = "0";
	entries["workflowinstance.savepoint.retry.wait"] = "2";
	entries["cluster.node.name"] = "localhost";
	entries["cluster.notify"] = "yes";
	entries["cluster.notify.user"] = "";
	entries["cluster.notify.password"] = "";
	entries["cluster.nodes"] = "";
	entries["cluster.cnx.timeout"] = "10";
	entries["cluster.rcv.timeout"] = "5";
	entries["cluster.snd.timeout"] = "5";
	entries["git.repository"] = "";
	entries["git.user"] = "";
	entries["git.password"] = "";
	entries["git.signature.name"] = "evQueue";
	entries["git.signature.email"] = "evqueue@local";
	entries["git.workflows.subdirectory"] = "workflows";
	entries["git.tasks.subdirectory"] = "tasks";
	
	instance=this;
}

Configuration::~Configuration(void)
{
	instance = 0;
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
		throw Exception("Configuration","Unknown configuration entry");
	return it->second;
}

int Configuration::GetInt(const string &entry) const
{
	const string value = Get(entry);
	if(value.substr(0,2)=="0x")
		return strtol(value.c_str(),0,16);
	return strtol(value.c_str(),0,10);
}

bool Configuration::GetBool(const string &entry) const
{
	const string value = Get(entry);
	if(value=="yes" || value=="true" || value=="1")
		return true;
	return false;
}

void Configuration::Substitute(void)
{
	map<string,string>::iterator it;
	for(it=entries.begin();it!=entries.end();it++)
	{
		pcrecpp::RE regex("(\\{[a-zA-Z_]+\\})");
		pcrecpp::StringPiece str_to_match(it->second);
		std::string match;
		while(regex.FindAndConsume(&str_to_match,&match))
		{
			string var = match;
			string var_name =  var.substr(1,var.length()-2);
			
			const char *value = getenv(var_name.c_str());
			if(value)
			{
				size_t start_pos = it->second.find(var);
				it->second.replace(start_pos,var.length(),string(value));
			}
		}
	}
}

void Configuration::SendConfiguration(QueryResponse *response)
{
	char buf[16];
	
	DOMDocument *xmldoc = response->GetDOM();
	
	DOMElement configuration_node = xmldoc->createElement("configuration");
	xmldoc->getDocumentElement().appendChild(configuration_node);
	
	
	map<string,string>::iterator it;
	for(it=entries.begin();it!=entries.end();it++)
	{
		DOMElement entry_node = xmldoc->createElement("entry");
		entry_node.setAttribute("name",it->first);
		if(it->first=="mysql.password" || it->first=="cluster.notify.password" || it->first=="git.password")
			entry_node.setAttribute("value","****"); // Do not send password
		else
			entry_node.setAttribute("value",it->second);
		configuration_node.appendChild(entry_node);
	}
}