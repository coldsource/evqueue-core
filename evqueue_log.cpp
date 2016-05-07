#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
	char *evqueue_log_filename = getenv("EVQUEUE_LOG_FILE");
	
	if(!evqueue_log_filename)
		return -1;
	
	FILE *f = fopen(evqueue_log_filename,"a+");
	if(!f)
		return -1;
	
	fseek(f,0,SEEK_END);
	if(ftell(f))
		fputc('\n',f);
	
	for(int i=1;i<argc;i++)
	{
		if(i>1)
			fputc(' ',f);
		
		fputs(argv[i],f);
	}
	
	fclose(f);
	
	return 0;
}