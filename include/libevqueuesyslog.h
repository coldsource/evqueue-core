#ifndef _LIBEVQUEUESYSLOG_H_
#define _LIBEVQUEUESYSLOG_H_

#include <stdio.h>
#include <stdarg.h>

extern FILE *evqueue_log;
extern int evqueue_default_facility;

void evqueue_syslog_init();
void evqueue_syslog(int priority,const char *fmt, va_list ap);
void openlog(const char *ident, int option, int facility);
void syslog(int priority, const char *format, ...);
void __syslog_chk (int pri, int flag, const char *fmt, ...);
void vsyslog(int priority, const char *format, va_list ap);

#endif