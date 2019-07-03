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
#include <sys/stat.h>
#include <unistd.h>
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
	entries["notifications.logs.directory"] = "/tmp";
	entries["notifications.logs.maxsize"] = "16K";
	entries["notifications.logs.delete"] = "yes";
	entries["processmanager.logs.delete"] = "yes";
	entries["processmanager.logs.directory"] = "/tmp";
	entries["processmanager.logs.tailsize"] = "20K";
	entries["processmanager.monitor.path"] = "/usr/bin/evqueue_monitor";
	entries["processmanager.monitor.ssh_key"] = "";
	entries["processmanager.monitor.ssh_path"] = "/usr/bin/ssh";
	entries["processmanager.agent.path"] = "/usr/bin/evqueue_agent";
	entries["processmanager.tasks.directory"] = ".";
	entries["datastore.dom.maxsize"] = "500K";
	entries["datastore.db.maxsize"] = "50M";
	entries["datastore.gzip.level"] = "9";
	entries["datastore.gzip.enable"] = "yes";
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
	entries["git.public_key"] = "";
	entries["git.private_key"] = "";
	entries["git.signature.name"] = "evQueue";
	entries["git.signature.email"] = "evqueue@local";
	entries["git.workflows.subdirectory"] = "workflows";
	
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

void Configuration::Check(void)
{
	check_f_is_exec(entries["processmanager.monitor.path"]);
	check_f_is_exec(entries["notifications.monitor.path"]);
	check_d_is_writeable(entries["processmanager.logs.directory"]);
	check_d_is_writeable(entries["notifications.logs.directory"]);

	check_bool_entry("core.auth.enable");
	check_bool_entry("core.fastshutdown");
	check_bool_entry("gc.enable");
	check_bool_entry("logger.db.enable");
	check_bool_entry("logger.syslog.enable");
	check_bool_entry("loggerapi.enable");
	check_bool_entry("processmanager.logs.delete");
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

	if(GetInt("workflowinstance.savepoint.level")>3)
		throw Exception("Configuration","workflowinstance.savepoint.level: invalid value '"+entries["workflowinstance.savepoint.level"]+"'. Value must be between O and 3");

	if(Get("queuepool.scheduler")!="fifo" && Get("queuepool.scheduler")!="prio")
		throw Exception("Configuration","queuepool.scheduler: invalid value '"+entries["queuepool.scheduler"]+"'. Value muse be 'fifo' or 'prio'");
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
