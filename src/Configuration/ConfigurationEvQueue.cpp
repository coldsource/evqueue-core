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

#include <Configuration/ConfigurationEvQueue.h>
#include <Exception/Exception.h>
#include <API/QueryResponse.h>
#include <DOM/DOMDocument.h>

#include <string.h>
#include <stdlib.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static auto init = Configuration::GetInstance()->RegisterConfig(new ConfigurationEvQueue());

using namespace std;

ConfigurationEvQueue::ConfigurationEvQueue(void)
{
	// Load default configuration
	entries["core.ipc.qid"] = "0xEA023E3C";
	entries["core.gid"] = "0";
	entries["core.pidfile"] = "/tmp/evqueue-core.pid";
	entries["core.uid"] = "0";
	entries["core.wd"] = "";
	entries["core.locale"] = "C.UTF-8";
	entries["core.auth.enable"] = "yes";
	entries["core.fastshutdown"] = "yes";
	entries["forker.pidfile"] = "/tmp/evqueue-forker.pid";
	entries["dpd.interval"] = "10";
	entries["queuepool.scheduler"] = "fifo";
	entries["gc.delay"] = "2";
	entries["gc.enable"] = "yes";
	entries["gc.interval"] = "43200";
	entries["gc.limit"] = "1000";
	entries["gc.logsapi.retention"] = "30";
	entries["gc.logsnotifications.retention"] = "30";
	entries["gc.logs.retention"] = "7";
	entries["gc.workflowinstance.retention"] = "30";
	entries["gc.uniqueaction.retention"] = "30";
	entries["logger.db.enable"] = "yes";
	entries["logger.db.filter"] = "LOG_WARNING";
	entries["logger.syslog.enable"] = "yes";
	entries["logger.syslog.filter"] = "LOG_NOTICE";
	entries["loggerapi.enable"] = "yes";
	entries["mysql.database"] = "evqueue";
	entries["mysql.host"] = "localhost";
	entries["mysql.password"] = "";
	entries["mysql.user"] = "";
	entries["network.bind.ip"] = "127.0.0.1";
	entries["network.bind.path"] = "";
	entries["network.bind.port"] = "5000";
	entries["network.connections.max"] = "128";
	entries["network.listen.backlog"] = "64";
	entries["network.rcv.timeout"] = "30";
	entries["network.snd.timeout"] = "30";
	entries["notifications.tasks.directory"] = "/tmp";
	entries["notifications.tasks.timeout"] = "5";
	entries["notifications.tasks.concurrency"] = "16";
	entries["notifications.logs.directory"] = "/tmp";
	entries["notifications.logs.maxsize"] = "16K";
	entries["notifications.logs.delete"] = "yes";
	entries["processmanager.logs.delete"] = "yes";
	entries["processmanager.logs.directory"] = "/tmp";
	entries["processmanager.logs.tailsize"] = "20K";
	entries["processmanager.monitor.ssh_key"] = "";
	entries["processmanager.monitor.ssh_path"] = "/usr/bin/ssh";
	entries["processmanager.agent.path"] = "/usr/bin/evqueue_agent";
	entries["processmanager.tasks.directory"] = ".";
	entries["processmanager.scripts.directory"] = "/tmp";
	entries["processmanager.scripts.delete"] = "yes";
	entries["forker.pipes.directory"] = "/tmp";
	entries["datastore.dom.maxsize"] = "500K";
	entries["datastore.db.maxsize"] = "50M";
	entries["datastore.gzip.level"] = "9";
	entries["datastore.gzip.enable"] = "yes";
	entries["workflowinstance.saveparameters"] = "yes";
	entries["workflowinstance.savepoint.level"] = "2";
	entries["workflowinstance.savepoint.retry.enable"] = "yes";
	entries["workflowinstance.savepoint.retry.times"] = "2";
	entries["workflowinstance.savepoint.retry.wait"] = "2";
	entries["cluster.node.name"] = "localhost";
	entries["cluster.notify"] = "yes";
	entries["cluster.notify.user"] = "";
	entries["cluster.notify.password"] = "";
	entries["cluster.nodes"] = "";
	entries["cluster.cnx.timeout"] = "10";
	entries["cluster.rcv.timeout"] = "5";
	entries["cluster.snd.timeout"] = "5";
}

ConfigurationEvQueue::~ConfigurationEvQueue(void)
{
}

void ConfigurationEvQueue::Check(void)
{
	check_d_is_writeable(entries["processmanager.logs.directory"]);
	check_d_is_writeable(entries["notifications.logs.directory"]);
	check_d_is_writeable(entries["notifications.tasks.directory"]);

	check_bool_entry("core.auth.enable");
	check_bool_entry("core.fastshutdown");
	check_bool_entry("gc.enable");
	check_bool_entry("logger.db.enable");
	check_bool_entry("logger.syslog.enable");
	check_bool_entry("loggerapi.enable");
	check_bool_entry("processmanager.logs.delete");
	check_bool_entry("processmanager.scripts.delete");
	check_bool_entry("notifications.logs.delete");
	check_bool_entry("datastore.gzip.enable");
	check_bool_entry("workflowinstance.saveparameters");
	check_bool_entry("workflowinstance.savepoint.retry.enable");
	check_bool_entry("cluster.notify");

	check_int_entry("dpd.interval");
	check_int_entry("gc.delay");
	check_int_entry("gc.interval");
	check_int_entry("gc.limit");
	check_int_entry("gc.logsapi.retention");
	check_int_entry("gc.logsnotifications.retention");
	check_int_entry("gc.logs.retention");
	check_int_entry("gc.workflowinstance.retention");
	check_int_entry("gc.uniqueaction.retention");
	check_int_entry("network.connections.max");
	check_int_entry("network.listen.backlog");
	check_int_entry("network.rcv.timeout");
	check_int_entry("network.snd.timeout");
	check_int_entry("network.bind.port");
	check_int_entry("notifications.tasks.timeout");
	check_int_entry("notifications.tasks.concurrency");
	check_int_entry("cluster.cnx.timeout");
	check_int_entry("cluster.rcv.timeout");
	check_int_entry("cluster.snd.timeout");
	check_int_entry("datastore.gzip.level");
	check_int_entry("workflowinstance.savepoint.level");
	check_int_entry("workflowinstance.savepoint.retry.times");
	check_int_entry("workflowinstance.savepoint.retry.wait");

	check_size_entry("processmanager.logs.tailsize");
	check_size_entry("datastore.dom.maxsize");
	check_size_entry("datastore.db.maxsize");
	check_size_entry("notifications.logs.maxsize");

	if(GetInt("datastore.gzip.level")<0 || GetInt("datastore.gzip.level")>9)
		throw Exception("Configuration","datastore.gzip.level: invalid value '"+entries["datastore.gzip.level"]+"'. Value must be between 0 and 9");
	
	if(GetInt("workflowinstance.savepoint.level")<0 || GetInt("workflowinstance.savepoint.level")>3)
		throw Exception("Configuration","workflowinstance.savepoint.level: invalid value '"+entries["workflowinstance.savepoint.level"]+"'. Value must be between O and 3");

	if(Get("queuepool.scheduler")!="fifo" && Get("queuepool.scheduler")!="prio")
		throw Exception("Configuration","queuepool.scheduler: invalid value '"+entries["queuepool.scheduler"]+"'. Value muse be 'fifo' or 'prio'");
}

void ConfigurationEvQueue::SendConfiguration(QueryResponse *response)
{
	DOMDocument *xmldoc = response->GetDOM();
	
	DOMElement configuration_node = xmldoc->createElement("configuration");
	xmldoc->getDocumentElement().appendChild(configuration_node);
	
	
	map<string,string>::iterator it;
	for(it=entries.begin();it!=entries.end();it++)
	{
		DOMElement entry_node = xmldoc->createElement("entry");
		entry_node.setAttribute("name",it->first);
		if(it->first.size()>9 && it->first.substr(it->first.size()-9)==".password" )
			entry_node.setAttribute("value","****"); // Do not send password
		else
			entry_node.setAttribute("value",it->second);
		configuration_node.appendChild(entry_node);
	}
}
