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

#include <ConfigurationReader.h>
#include <Exception.h>

#include <stdio.h>
#include <ctype.h>
#include <string.h>

Configuration *ConfigurationReader::Read(const char *filename)
{
	FILE *f;
	int i,len,entry_len,value_len;
	int quoted;
	char line[CONFIGURATION_LINE_MAXLEN];
	char *entry,*value;
	Configuration *config;
	
	config=new Configuration();
	
	f=0;
	
	try
	{
		f=fopen(filename,"r");
		if(!f)
			throw Exception("ConfigurationReader","Unable to open configuration file");
		
		while(fgets(line,CONFIGURATION_LINE_MAXLEN,f))
		{
			len=strlen(line);
			
			// Trim line
			if(line[len-1]=='\n')
			{
				line[len-1]='\0';
				len--;
			}
			if(line[len-1]=='\r')
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
				throw Exception("ConfigurationReader","Expected configuration entry");
			
			if(line[i]=='\0')
				throw Exception("ConfigurationReader","Missing value");
			
			entry_len=(line+i)-entry;
			
			// Skip spaces
			while(line[i]==' ' || line[i]=='\t')
				i++;
			
			// =
			if(line[i++]!='=')
				throw Exception("ConfigurationReader","Expecting '='");
			
			// Skip spaces
			while(line[i]==' ' || line[i]=='\t')
				i++;
			
			if(line[i]=='\0')
				throw Exception("ConfigurationReader","Missing value");
			
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
				throw Exception("ConfigurationReader","Empty value");
			
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
				throw Exception("ConfigurationReader","Missing ending quote");
			
			if(quoted>0)
				i++;
			
			while(line[i]==' ' || line[i]=='\t')
				i++;
				
			if(line[i]!='\0' && line[i]!='#')
				throw Exception("ConfigurationReader","Garbage data after value");
			
			entry[entry_len]='\0';
			value[value_len]='\0';
			
			// Set configuration entry
			if(!config->Set(entry,value))
				throw Exception("ConfigurationReader","Unknown configuration entry");
		}
	}
	catch(Exception &e)
	{
		if(f)
			fclose(f);
		delete config;
		throw e;
	}
	
	fclose(f);
	
	return config;
}
