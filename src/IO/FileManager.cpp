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

#include <IO/FileManager.h>
#include <Exception/Exception.h>
#include <Logger/Logger.h>
#include <Crypto/base64.h>
#include <Crypto/sha1.h>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

using namespace std;

bool FileManager::CheckFileName(const std::string &directory,const string &file_name)
{
	if(file_name.length()==0)
		return false;
	
	if(file_name.substr(0,3)=="../")
		return false;
	
	if(file_name.find("/../")!=string::npos)
		return false;
	
	for(int i=0;i<file_name.length();i++)
		if(!isalnum(file_name[i]) && file_name[i]!='_' && file_name[i]!='-' && file_name[i]!='.' && file_name[i]!='@' && file_name[i]!='/')
			return false;
	
	return true;
}

void FileManager::PutFile(const string &directory,const string &filename,const string &data,int filetype,int datatype)
{
	if(!CheckFileName(directory,filename))
		throw Exception("FileManager","Invalid file name");
	
	string path = directory+"/"+filename;
	
	int fd;
	if(filetype==FILETYPE_CONF)
		fd = open(path.c_str(),O_CREAT|O_RDWR|O_TRUNC,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	else if(filetype==FILETYPE_BINARY)
		fd = open(path.c_str(),O_CREAT|O_EXCL|O_RDWR,S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
	
	if(fd==-1)
	{
		if(errno==EEXIST)
		{
			Logger::Log(LOG_ERR,"File already exist (and will not be overridden) : %s",path.c_str());
			throw Exception("FileManager","File already exist");
		}
		
		Logger::Log(LOG_ERR,"Unable to create file : %s",path.c_str());
		throw Exception("FileManager","Unable to create file");
	}
	
	FILE *f = fdopen(fd,"w");
	bool re;
	if(datatype==DATATYPE_BASE64)
		re = base64_decode_file(f,data);
	else if(datatype==DATATYPE_BINARY)
		re = fwrite(data.c_str(),1,data.length(),f);
	fclose(f);
	
	if(!re)
	{
		unlink(path.c_str());
		throw Exception("FileManager","Invalid file data in file "+filename);
	}
}

void FileManager::GetFile(const string &directory,const string &filename,string &data)
{
	if(!CheckFileName(directory,filename))
		throw Exception("FileManager","Invalid file name");
	
	string path = directory+"/"+filename;
	
	int fd = open(path.c_str(),O_RDONLY);
	if(fd==-1)
	{
		Logger::Log(LOG_ERR,"Unable to open file : %s",path.c_str());
		throw Exception("FileManager","Unable to open file");
	}
	
	FILE *f = fdopen(fd,"r");
	base64_encode_file(f,data);
	fclose(f);
}

void FileManager::GetFileHash(const string &directory,const string &filename,string &hash)
{
	if(!CheckFileName(directory,filename))
		throw Exception("FileManager","Invalid file name");
	
	string path = directory+"/"+filename;
	
	int fd = open(path.c_str(),O_RDONLY);
	if(fd==-1)
	{
		Logger::Log(LOG_NOTICE,"Unable to open file : %s",path.c_str());
		throw Exception("FileManager","Unable to open file");
	}
	
	FILE *f = fdopen(fd,"r");
	
	char c_hash[20];
	sha1_stream(f,c_hash);
	hash.clear();
	hash.append(c_hash,20);
	
	fclose(f);
}

void FileManager::RemoveFile(const string &directory,const string &filename)
{
	if(!CheckFileName(directory,filename))
		throw Exception("FileManager","Invalid file name");
	
	string path = directory+"/"+filename;
	
	int re = unlink(path.c_str());
	if(re!=0)
	{
		Logger::Log(LOG_ERR,"Unable to remove file : %s",path.c_str());
		throw Exception("FileManager","Unable to remove file");
	}
}

void FileManager::Chmod(const string &directory,const string &filename,mode_t mode)
{
	string path = directory+"/"+filename;
	int re = chmod(path.c_str(),mode);
	if(re!=0)
	{
		Logger::Log(LOG_ERR,"Unable to chmod file : %s",path.c_str());
		throw Exception("FileManager","Unable to chmod file");
	}
}
