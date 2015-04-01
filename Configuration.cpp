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

#include <string.h>
#include <stdlib.h>

#define CONFIGURATION_ENTRIES 32

static const char *default_configuration[]={
	"core.gid","0",
	"core.uid","0",
	"gc.delay","2",
	"gc.enable","yes",
	"gc.interval","43200",
	"gc.limit","1000",
	"gc.logs.retention","7",
	"gc.workflowinstance.retention","30",
	"logger.db.enable","no",
	"logger.syslog.enable","yes",
	"mysql.database","queueing",
	"mysql.host","localhost",
	"mysql.password","",
	"mysql.user","",
	"network.bind.ip","127.0.0.1",
	"network.bind.port","5000",
	"network.listen.backlog","64",
	"network.rcv.timeout","30",
	"network.snd.timeout","30",
	"processmanager.errlogs.directory","errlogs",
	"processmanager.errlogs.enable","no",
	"processmanager.logs.delete","yes",
	"processmanager.logs.directory","/tmp",
	"processmanager.monitor.path","/usr/local/bin/evqueue_monitor",
	"processmanager.monitor.ssh_key","",
	"processmanager.monitor.ssh_path","/usr/bin/ssh",
	"processmanager.tasks.directory",".",
	"workflowinstance.saveparameters","yes",
	"workflowinstance.savepoint.level","2",
	"workflowinstance.savepoint.retry.enable","yes",
	"workflowinstance.savepoint.retry.times","0",
	"workflowinstance.savepoint.retry.wait","2",
	};

Configuration *Configuration::instance=0;

Configuration::Configuration(void)
{
	entries=new char*[CONFIGURATION_ENTRIES];
	values=new char*[CONFIGURATION_ENTRIES];
	
	for(int i=0;i<CONFIGURATION_ENTRIES;i++)
	{
		entries[i]=new char[strlen(default_configuration[i*2])+1];
		strcpy(entries[i],default_configuration[i*2]);
		
		values[i]=new char[strlen(default_configuration[i*2+1])+1];
		strcpy(values[i],default_configuration[i*2+1]);
	}
	
	instance=this;
}

Configuration::~Configuration(void)
{
	for(int i=0;i<CONFIGURATION_ENTRIES;i++)
	{
		delete[] entries[i];
		delete[] values[i];
	}
	
	delete[] entries;
	delete[] values;
}

int Configuration::lookup(const char *entry)
{
	int cmp,begin,middle,end;
	
	begin=0;
	end=CONFIGURATION_ENTRIES-1;
	do
	{
		middle=(end+begin)/2;
		cmp=strcmp(entries[middle],entry);
		if(cmp==0)
			return middle;
		if(cmp<0)
			begin=middle+1;
		else
			end=middle-1;
	}while(begin<=end);
	
	return -1;
}

bool Configuration::Set(const char *entry,const char *value)
{
	int i;
	
	i=lookup(entry);
	if(i==-1)
		return false;
	
	delete[] values[i];
	values[i]=new char[strlen(value)+1];
	strcpy(values[i],value);
	
	return true;
}

const char *Configuration::Get(const char *entry)
{
	int i;
	
	i=lookup(entry);
	if(i==-1)
		return 0;
	
	return values[i];
}

int Configuration::GetInt(const char *entry)
{
	const char *value;
	value=Get(entry);
	if(value==0)
		return -1;
	return atoi(value);
}

bool Configuration::GetBool(const char *entry)
{
	const char *value;
	value=Get(entry);
	if(strcasecmp(value,"yes")==0 || strcasecmp(value,"true")==0 || strcasecmp(value,"1")==0)
		return true;
	return false;
}
