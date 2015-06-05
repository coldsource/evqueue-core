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

#include <xqilla/xqilla-dom3.hpp>
#include <xercesc/dom/DOM.hpp>

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>

using namespace std;
using namespace xercesc;

Configuration *Configuration::instance=0;

Configuration::Configuration(void)
{
	// Load default configuration
	entries["core.gid"] = "0";
	entries["core.pidfile"] = "/var/run/evqueue/evqueue.pid";
	entries["core.uid"] = "0";
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
	entries["network.bind.ip"] = "127.0.0.1";
	entries["network.bind.path"] = "/var/run/evqueue/evqueue.socket";
	entries["network.bind.port"] = "5000";
	entries["network.connections.max"] = "128";
	entries["network.listen.backlog"] = "64";
	entries["network.rcv.timeout"] = "30";
	entries["network.snd.timeout"] = "30";
	entries["notifications.monitor.path"] = "/usr/bin/evqueue_notification_monitor";
	entries["notifications.tasks.directory"] = "/usr/share/evqueue/plugins/notifications";
	entries["notifications.tasks.timeout"] = "5";
	entries["notifications.tasks.concurrency"] = "32";
	entries["processmanager.errlogs.directory"] = "errlogs";
	entries["processmanager.errlogs.enable"] = "no";
	entries["processmanager.logs.delete"] = "yes";
	entries["processmanager.logs.directory"] = "/tmp";
	entries["processmanager.monitor.path"] = "/usr/bin/evqueue_monitor";
	entries["processmanager.monitor.ssh_key"] = "";
	entries["processmanager.monitor.ssh_path"] = "/usr/bin/ssh";
	entries["processmanager.tasks.directory"] = ".";
	entries["workflowinstance.saveparameters"] = "yes";
	entries["workflowinstance.savepoint.level"] = "2";
	entries["workflowinstance.savepoint.retry.enable"] = "yes";
	entries["workflowinstance.savepoint.retry.times"] = "0";
	entries["workflowinstance.savepoint.retry.wait"] = "2";
	
	instance=this;
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
	return atoi(Get(entry).c_str());
}

bool Configuration::GetBool(const string &entry) const
{
	const string value = Get(entry);
	if(value=="yes" || value=="true" || value=="1")
		return true;
	return false;
}

void Configuration::SendConfiguration(int s)
{
	char buf[16];
	
	DOMImplementation *xqillaImplementation = DOMImplementationRegistry::getDOMImplementation(X("XPath2 3.0"));
	DOMDocument *xmldoc = xqillaImplementation->createDocument();
	
	DOMElement *configuration_node = xmldoc->createElement(X("configuration"));
	xmldoc->appendChild(configuration_node);
	
	
	map<string,string>::iterator it;
	for(it=entries.begin();it!=entries.end();it++)
	{
		DOMElement *entry_node = xmldoc->createElement(X("entry"));
		entry_node->setAttribute(X("name"),X(it->first.c_str()));
		entry_node->setAttribute(X("value"),X(it->second.c_str()));
		configuration_node->appendChild(entry_node);
	}
	
	DOMLSSerializer *serializer = xqillaImplementation->createLSSerializer();
	XMLCh *configuration_xml = serializer->writeToString(configuration_node);
	char *configuration_xml_c = XMLString::transcode(configuration_xml);
	
	send(s,configuration_xml_c,strlen(configuration_xml_c),0);
	
	XMLString::release(&configuration_xml);
	XMLString::release(&configuration_xml_c);
	serializer->release();
	xmldoc->release();
}