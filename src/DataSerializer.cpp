#include <DataSerializer.h>

#include <unistd.h>

using namespace std;

string DataSerializer::Serialize(const map<string,string> &map)
{
	string data;
	char buf[32];
	
	sprintf(buf,"%03ld",map.size());
	data += buf;
	
	for(auto it=map.begin();it!=map.end();++it)
	{
		sprintf(buf,"%09ld%09ld",it->first.length(),it->second.length());
		data += buf;
		data += it->first + it->second;
	}
	
	return data;
}

string DataSerializer::Serialize(const string &str)
{
	string data;
	char buf[32];
	
	sprintf(buf,"%09ld",str.length());
	data += buf;
	
	data += data;
	
	return data;
}

bool DataSerializer::Unserialize(int fd, map<string,string> &map)
{
	int read_size;
	char buf[4096];
	
	// Read the number of arguments
	read_size = read(fd,buf,3);
	if(read_size!=3)
		return false;
		
	buf[read_size] = '\0';
	int nparameters = atoi(buf);
	
	for(int i=0;i<nparameters;i++)
	{
		// Read the number of arguments
		read_size = read(fd,buf,18);
		if(read_size!=18)
			return false;
			
		buf[read_size] = '\0';
		int value_len = atoi(buf+9);
		
		buf[9] = '\0';
		int name_len = atoi(buf);
		
		if(value_len>4096 || name_len>4096)
			return false;
		
		char name[4097],value[4097];
		read_size = read(fd,name,name_len);
		if(read_size!=name_len)
			return false;
		
		name[read_size] = '\0';
		
		read_size = read(fd,value,value_len);
		if(read_size!=value_len)
			return false;
		
		value[read_size] = '\0';
		
		if(name[0]!='\0')
			map[name] = value;
		else
			map["DEFAULT_PARAMETER_NAME"] = value;
	}
	
	return true;
}

bool DataSerializer::Unserialize(int fd, string &str)
{
	int read_size;
	char buf[32];
	
	read_size = read(fd,buf,9);
	if(read_size!=9)
		return false;
	
	buf[read_size] = '\0';
	int value_len = atoi(buf);
	
	char value[value_len];
	read_size = read(fd,value,value_len);
	if(read_size!=value_len)
		return false;
	
	str.assign(value,value_len);
	
	return true;
}
