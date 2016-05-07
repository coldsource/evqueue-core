#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <syslog.h>

FILE *evqueue_log = 0;
int evqueue_default_facility = 0;

void evqueue_syslog_init()
{
	char *log_filename;
	
	log_filename = getenv("EVQUEUE_SYSLOG_FILE");
	if(!log_filename)
		return;
	
	evqueue_log = fopen(log_filename,"a+");
}

void evqueue_syslog(int priority,const char *fmt, va_list ap)
{
	if(!evqueue_log)
		evqueue_syslog_init();
	
	if(!evqueue_log)
		return;
	
	int facility = priority&0xFFFFFFF8;
	if(!facility)
		facility = evqueue_default_facility;
	
	priority = priority&0x07;
	
	if(facility==LOG_LOCAL7)
	{
		char buf[16];
		int n;
		if(vsnprintf(buf,16,fmt,ap) && sscanf(buf,"%d",&n)==1)
		{
			fprintf(evqueue_log,"done %d %%\n",n);
			return;
		}
	}
	
	if(priority==LOG_EMERG)
		fprintf(evqueue_log,"EMERG");
	else if(priority==LOG_ALERT)
		fprintf(evqueue_log,"ALERT");
	else if(priority==LOG_CRIT)
		fprintf(evqueue_log,"CRIT");
	else if(priority==LOG_ERR)
		fprintf(evqueue_log,"ERR");
	else if(priority==LOG_WARNING)
		fprintf(evqueue_log,"WARNING");
	else if(priority==LOG_NOTICE)
		fprintf(evqueue_log,"NOTICE");
	else if(priority==LOG_INFO)
		fprintf(evqueue_log,"INFO");
	else if(priority==LOG_DEBUG)
		fprintf(evqueue_log,"DEBUG");
	
	fprintf(evqueue_log,": ");
	
	vfprintf(evqueue_log,fmt,ap);
	
	fprintf(evqueue_log,"\n");
}

void openlog(const char *ident, int option, int facility)
{
	evqueue_default_facility = facility;
}

void syslog(int priority, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	evqueue_syslog(priority,format,ap);
	va_end(ap);
}

void __syslog_chk (int pri, int flag, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	evqueue_syslog(pri,fmt,ap);
	va_end(ap);
}

void vsyslog(int priority, const char *format, va_list ap)
{
	evqueue_syslog(priority,format,ap);
}
