#include "common.h"
#include <time.h>

static int second;
static int minute;
static int hour;
static int day;
static int month;
static int year;

void read_rtc();
const char *os_asctime()
{
	static char buffer[128];
	memset(buffer, 0, 128);
	read_rtc();
	sprintf(buffer, "%02d:%02d:%02d", hour, minute, second);
	return buffer;
}
const char *asciidate()
{
	static char buffer[128];
	memset(buffer, 0, 128);
	read_rtc();
	sprintf(buffer, " %02d/%02d/%04d",  day, month, year);
	return buffer;
}
const char *asciidatetime()
{
	static char buffer[128];
	memset(buffer, 0, 128);
	read_rtc();
	sprintf(buffer, "%02d:%02d:%02d -- %02d/%02d/%04d", hour, minute, second, day, month, year);
	return buffer;
}
enum
{
	cmos_address = 0x70,
	cmos_data = 0x71
};

int get_update_in_progress_flag()
{
	outb(cmos_address, 0x0A);
	return (inb(cmos_data) & 0x80);
}

unsigned char get_RTC_register(int reg)
{
	outb(cmos_address, reg);
	return inb(cmos_data);
}

void read_rtc()
{
	unsigned char century;
	unsigned char registerB;

	// Make sure an update isn't in progress
	while (get_update_in_progress_flag())
	{
	}

	second = get_RTC_register(0x00);
	minute = get_RTC_register(0x02);
	hour = get_RTC_register(0x04);
	day = get_RTC_register(0x07);
	month = get_RTC_register(0x08);
	year = get_RTC_register(0x09);
	century = get_RTC_register(0x01);
	registerB = get_RTC_register(0x0B);

	// Convert BCD to binary values if necessary
	if (!(registerB & 0x04))
	{
		second = (second & 0x0F) + ((second / 16) * 10);
		minute = (minute & 0x0F) + ((minute / 16) * 10);
		hour = ((hour & 0x0F) + (((hour & 0x70) / 16) * 10)) | (hour & 0x80);
		day = (day & 0x0F) + ((day / 16) * 10);
		month = (month & 0x0F) + ((month / 16) * 10);
		year = (year & 0x0F) + ((year / 16) * 10);
		century = (century & 0x0F) + ((century / 16) * 10);
	}

	// Calculate the full (4-digit) year
	year += 2000 + (century * 100);
}

static int is_leap_year(int yr)
{
	return yr % 4 == 0 && (yr % 100 != 0 || year % 400 == 0);
}
static int day_of_week(int d, int m, int y)
{
	int wday = (d += m < 3 ? y-- : y - 2, 23 * m / 9 + d + 4 + y / 4 - y / 100 + y / 400) % 7;
	return wday;
}
struct tm *localtime(const time_t *time)
{
	static int daysToMonth[2][12] =
		{
			{0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
			{0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335},
		};

	read_rtc();
	static struct tm lt;
	lt.tm_sec = second;
	lt.tm_min = minute;
	lt.tm_hour = hour;
	lt.tm_mday = day;
	lt.tm_mon = month;
	lt.tm_wday = day_of_week(day, month, year);
	lt.tm_yday = daysToMonth[is_leap_year(year) ? 1 : 0][month - 1] + day - 1;
	lt.tm_year = year;
	lt.tm_isdst = -1;
	return &lt;
}

time_t time(time_t *arg)
{
	struct tm *lt = localtime(0);

	int tm_sec = lt->tm_sec;
	int tm_min = lt->tm_min;
	int tm_hour = lt->tm_hour;
	int tm_yday = lt->tm_yday;
	int tm_year = lt->tm_year - 1900;
	//	printf("<%d:%d:%d:%d>\n",tm_year,tm_yday,lt->tm_wday,lt->tm_mon);
	int seconds = tm_sec + tm_min * 60 + tm_hour * 3600 + tm_yday * 86400 +
				  (tm_year - 70) * 31536000 + ((tm_year - 69) / 4) * 86400 -
				  ((tm_year - 1) / 100) * 86400 + ((tm_year + 299) / 400) * 86400;

	if (arg)
	{
		*arg = seconds;
	}
	return seconds;
}

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
	struct tm *lt = localtime(0);

	int tm_sec = lt->tm_sec;
	int tm_min = lt->tm_min;
	int tm_hour = lt->tm_hour;
	int tm_yday = lt->tm_yday;
	int tm_year = lt->tm_year - 1900;
	//	printf("<%d:%d:%d:%d>\n",tm_year,tm_yday,lt->tm_wday,lt->tm_mon);
	int seconds = tm_sec + tm_min * 60 + tm_hour * 3600 + tm_yday * 86400 +
				  (tm_year - 70) * 31536000 + ((tm_year - 69) / 4) * 86400 -
				  ((tm_year - 1) / 100) * 86400 + ((tm_year + 299) / 400) * 86400;

	int32_t currentticks = get_tick_count();
	tv->tv_sec = seconds; // currentticks/1000;
	tv->tv_usec = (currentticks % 1000) * 1000;
	tz->tz_minuteswest = 0;
	tz->tz_dsttime = 0;
	return 0;
}
