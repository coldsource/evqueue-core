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

#include <Configuration/ConfigurationReader.h>
#include <Exception/Exception.h>

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <map>

using namespace std;

void ConfigurationReader::Read(const string &filename, Configuration *config)
{
	FILE *f;
	int i,len,entry_len,value_len,lineno = 0;
	int quoted;
	char line[CONFIGURATION_LINE_MAXLEN];
	char *entry,*value;
	
	f=0;
	
	try
	{
		f=fopen(filename.c_str(),"r");
		if(!f)
			throw Exception("ConfigurationReader","Unable to open configuration file");
		
		while(fgets(line,CONFIGURATION_LINE_MAXLEN,f))
		{
			lineno++;
			
			len=strlen(line);
			
			// Trim line
			if(len && line[len-1]=='\n')
			{
				line[len-1]='\0';
				len--;
			}
			if(len && line[len-1]=='\r')
			{
				line[len-1]='\0';
				len--;
			}
			
			// Empty line
			if(len==0)
				continue;
			
			// Skip spaces
			i=0;
			while(line[i]==' ' || line[i]=='\t')
				i++;
			
			// Comment line
			if(line[i]=='#' || line[i]=='\0')
				continue;
			
			// Read value
			entry=line+i;
			while(isalnum(line[i]) || line[i]=='.' || line[i]=='_')
				i++;
			
			if(entry==line+i)
				throw Exception("ConfigurationReader","Expected configuration entry on line "+to_string(lineno));
			
			if(line[i]=='\0')
				throw Exception("ConfigurationReader","Missing value on line "+to_string(lineno));
			
			entry_len=(line+i)-entry;
			
			// Skip spaces
			while(line[i]==' ' || line[i]=='\t')
				i++;
			
			// =
			if(line[i++]!='=')
				throw Exception("ConfigurationReader","Expecting '=' on line "+to_string(lineno));
			
			// Skip spaces
			while(line[i]==' ' || line[i]=='\t')
				i++;
			
			if(line[i]=='\0')
				throw Exception("ConfigurationReader","Missing value on line "+to_string(lineno));
			
			// Check if value is quoted
			quoted=0;
			if(line[i]=='\'')
			{
				quoted=1;
				i++;
			}
			else if(line[i]=='\"')
			{
				quoted=2;
				i++;
			}
			
			if(line[i]=='\0')
				throw Exception("ConfigurationReader","Empty value on line "+to_string(lineno));
			
			// Read value
			value=line+i;
			while(line[i]!='\0')
			{
				if(quoted==0 && line[i]==' ' || line[i]=='\t' || line[i]=='#')
					break;
				else if(quoted==1 && line[i]=='\'')
					break;
				else if(quoted==2 && line[i]=='\"')
					break;
				
				i++;
			}
			
			value_len=(line+i)-value;
			
			if(quoted>0 && line[i]=='\0')
				throw Exception("ConfigurationReader","Missing ending quote on line "+to_string(lineno));
			
			if(quoted>0)
				i++;
			
			while(line[i]==' ' || line[i]=='\t')
				i++;
				
			if(line[i]!='\0' && line[i]!='#')
				throw Exception("ConfigurationReader","Garbage data after value on line "+to_string(lineno));
			
			entry[entry_len]='\0';
			value[value_len]='\0';
			
			// Set configuration entry
			if(!config->Set(entry,value))
				throw Exception("ConfigurationReader","Unknown configuration entry : "+string(entry));
		}
	}
	catch(Exception &e)
	{
		if(f)
			fclose(f);
		throw e;
	}
	
	fclose(f);
}

void ConfigurationReader::ReadDefaultPaths(const string &filename, Configuration *config)
{
	{
		// Try to read global config from /etc
		string path = "/etc/"+filename;
		
		struct stat file_stats;
		if(stat(path.c_str(), &file_stats)==0)
			ConfigurationReader::Read(path.c_str(), config);
	}
	
	{
		// Try to read config from home directory
		char *home = getenv("HOME");
		if(home)
		{
			string path = home;
			path += "/."+filename;
			
			struct stat file_stats;
			if(stat(path.c_str(), &file_stats)==0)
				ConfigurationReader::Read(path, config);
		}
	}
}

int ConfigurationReader::ReadCommandLine(int argc, char **argv, const vector<string> &filter, const string &prefix, Configuration *config)
{
	// Compute type of each argument
	map<string, string> key_type_filter;
	for(int i=0;i<filter.size();i++)
	{
		string f = filter.at(i);
		string type;
		string key;
		
		if(f.substr(0,7)=="string:")
		{
			type = "string";
			key = f.substr(7);
		}
		else if(f.substr(0,5)=="bool:")
		{
			type = "bool";
			key = f.substr(5);
		}
		else
		{
			type= "string";
			key = f;
		}
		
		key_type_filter.insert(pair<string, string>(key, type));
	}
	
	// Read command line arguments and add them to the config
	int cur;
	for(cur=1;cur<argc;cur++)
	{
		string arg = argv[cur];
		if(arg.substr(0,2)!="--")
			return -1;
		
		if(arg=="--")
			return cur+1;
		
		string key = arg.substr(2);
		
		// Check if key is filtred
		auto it = key_type_filter.find(key);
		if(it==key_type_filter.end())
			return cur;
		string type = it->second;
		
		if(type=="string")
		{
			if(cur+1>=argc)
				return -1;
			
			config->Set(prefix+"."+key, argv[cur+1]);
			cur++;
		}
		else if(type=="bool")
		{
			config->Set(prefix+"."+key, "yes");
		}
	}
	
	return cur;
}
