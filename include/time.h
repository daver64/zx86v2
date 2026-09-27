
#ifndef _STDC_TIME_H_
#define _STDC_TIME_H_

#include <sys/types.h>

struct timeval
{
  time_t      tv_sec;     /* seconds */
  suseconds_t tv_usec;    /* microseconds */
};
struct timezone
{
  int tz_minuteswest;     /* minutes west of Greenwich */
  int tz_dsttime;         /* type of DST correction */
};
struct tm
{
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
};
time_t time( time_t *arg );
int gettimeofday(struct timeval *tv, struct timezone *tz);
struct tm *localtime(const time_t *time);
#endif

