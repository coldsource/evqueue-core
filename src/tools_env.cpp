#include <tools_env.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

bool prepare_env()
{
	// Register ENV parameters
	int read_size;
	char buf[4096];
	
	// Read the number of arguments
	read_size = read(STDIN_FILENO,buf,3);
	if(read_size!=3)
		return false;
		
	buf[read_size] = '\0';
	int nparameters = atoi(buf);
	
	for(int i=0;i<nparameters;i++)
	{
		// Read the number of arguments
		read_size = read(STDIN_FILENO,buf,18);
		if(read_size!=18)
			return false;
			
		buf[read_size] = '\0';
		int value_len = atoi(buf+9);
		
		buf[9] = '\0';
		int name_len = atoi(buf);
		
		if(value_len>4096 || name_len>4096)
			return false;
		
		char name[4097],value[4097];
		read_size = read(STDIN_FILENO,name,name_len);
		if(read_size!=name_len)
			return false;
		
		name[read_size] = '\0';
		
		read_size = read(STDIN_FILENO,value,value_len);
		if(read_size!=value_len)
			return false;
		
		value[read_size] = '\0';
		
		if(name[0]!='\0')
			setenv(name,value,1);
		else
			setenv("DEFAULT_PARAMETER_NAME",value,1);
	}
	
	return true;
}