/**************************************************
 * malloc/free/realloc wrap for gcc compiler
 *
 **************************************************/
#if defined(__GNUC__)
#include "FreeRTOS.h"

//wrap malloc to pvPortMalloc
void *__wrap_malloc(size_t size)
{
	return pvPortMalloc(size);
}

void *__wrap_realloc(void *p, size_t size)
{
	return pvPortReAlloc(p, size);
}

void __wrap_free(void *p)
{
	vPortFree(p);
}

void *__wrap_calloc(size_t xWantedCnt, size_t xWantedSize)
{
	return pvPortCalloc(xWantedCnt, xWantedSize);
}

/* For GCC stdlib */
void *__wrap__malloc_r(void *reent, size_t size)
{
	(void) reent;
	return pvPortMalloc(size);
}

void *__wrap__realloc_r(void *reent, void *p, size_t size)
{
	(void) reent;
	return pvPortReAlloc(p, size);
}

void __wrap__free_r(void *reent, void *p)
{
	(void) reent;
	vPortFree(p);
}

void *__wrap__calloc_r(void *reent, size_t xWantedCnt, size_t xWantedSize)
{
	(void) reent;
	return pvPortCalloc(xWantedCnt, xWantedSize);
}

#if (defined(CONFIG_SYSTEM_TIME64) && CONFIG_SYSTEM_TIME64)
/**************************************************
 * mktime/localtime wrap for gcc compiler
 * port from newlib v3.2.0
 * for 64 bit time
 **************************************************/

#define SECSPERMIN	60L
#define MINSPERHOUR	60L
#define HOURSPERDAY	24L
#define SECSPERHOUR	(SECSPERMIN * MINSPERHOUR)
#define SECSPERDAY	(SECSPERHOUR * HOURSPERDAY)
#define DAYSPERWEEK	7
#define MONSPERYEAR	12

#define YEAR_BASE	1900
#define EPOCH_YEAR      1970
#define EPOCH_WDAY      4
#define EPOCH_YEARS_SINCE_LEAP 2
#define EPOCH_YEARS_SINCE_CENTURY 70
#define EPOCH_YEARS_SINCE_LEAP_CENTURY 370

#define isleap(y) ((((y) % 4) == 0 && ((y) % 100) != 0) || ((y) % 400) == 0)

#define EPOCH_ADJUSTMENT_DAYS	719468L
/* year to which the adjustment was made */
#define ADJUSTED_EPOCH_YEAR	0
/* 1st March of year 0 is Wednesday */
#define ADJUSTED_EPOCH_WDAY	3
/* there are 97 leap years in 400-year periods. ((400 - 97) * 365 + 97 * 366) */
#define DAYS_PER_ERA		146097L
/* there are 24 leap years in 100-year periods. ((100 - 24) * 365 + 24 * 366) */
#define DAYS_PER_CENTURY	36524L
/* there is one leap year every 4 years */
#define DAYS_PER_4_YEARS	(3 * 365 + 366)
/* number of days in a non-leap year */
#define DAYS_PER_YEAR		365
/* number of days in January */
#define DAYS_IN_JANUARY		31
/* number of days in non-leap February */
#define DAYS_IN_FEBRUARY	28
/* number of years per era */
#define YEARS_PER_ERA		400

#define _SEC_IN_MINUTE 60L
#define _SEC_IN_HOUR 3600L
#define _SEC_IN_DAY 86400L

static const int DAYS_IN_MONTH[12] =
{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

#define _DAYS_IN_MONTH(x) ((x == 1) ? days_in_feb : DAYS_IN_MONTH[x])

static const int _DAYS_BEFORE_MONTH[12] =
{0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

#define _ISLEAP(y) (((y) % 4) == 0 && (((y) % 100) != 0 || (((y)+1900) % 400) == 0))
#define _DAYS_IN_YEAR(year) (_ISLEAP(year) ? 366 : 365)

#include "time64.h"
#include <platform_stdlib.h>

#if (__GNUC__ > 9)
extern long long  _Tzoff();

typedef struct __tzrule_struct {
	char ch;
	int m;
	int n;
	int d;
	int s;
	time_t change;
	long offset; /* Match type of _timezone. */
} __tzrule_type;

typedef struct __tzinfo_struct {
	int __tznorth;
	int __tzyear;
	__tzrule_type __tzrule[2];
} __tzinfo_type;

/* Shared timezone information for libc/time functions.  */
static __tzinfo_type tzinfo = {1, 0,
	{	{'J', 0, 0, 0, 0, (time_t)0, 0L },
		{'J', 0, 0, 0, 0, (time_t)0, 0L }
	}
};

__tzinfo_type *
__gettzinfo(void)
{
	return &tzinfo;
}
#endif

extern int __tzcalc_limits(int);

struct tm *
gmtime64_r(const time_t *__restrict tim_p,
		   struct tm *__restrict res)
{
	long days, rem;
	const time_t lcltime = *tim_p;
	int era, weekday, year;
	unsigned erayear, yearday, month, day;
	unsigned long eraday;

	days = lcltime / SECSPERDAY + EPOCH_ADJUSTMENT_DAYS;
	rem = lcltime % SECSPERDAY;
	if (rem < 0) {
		rem += SECSPERDAY;
		--days;
	}

	/* compute hour, min, and sec */
	res->tm_hour = (int)(rem / SECSPERHOUR);
	rem %= SECSPERHOUR;
	res->tm_min = (int)(rem / SECSPERMIN);
	res->tm_sec = (int)(rem % SECSPERMIN);

	/* compute day of week */
	if ((weekday = ((ADJUSTED_EPOCH_WDAY + days) % DAYSPERWEEK)) < 0) {
		weekday += DAYSPERWEEK;
	}
	res->tm_wday = weekday;

	/* compute year, month, day & day of year */
	/* for description of this algorithm see
	 * http://howardhinnant.github.io/date_algorithms.html#civil_from_days */
	era = (days >= 0 ? days : days - (DAYS_PER_ERA - 1)) / DAYS_PER_ERA;
	eraday = days - era * DAYS_PER_ERA;	/* [0, 146096] */
	erayear = (eraday - eraday / (DAYS_PER_4_YEARS - 1) + eraday / DAYS_PER_CENTURY -
			   eraday / (DAYS_PER_ERA - 1)) / 365;	/* [0, 399] */
	yearday = eraday - (DAYS_PER_YEAR * erayear + erayear / 4 - erayear / 100);	/* [0, 365] */
	month = (5 * yearday + 2) / 153;	/* [0, 11] */
	day = yearday - (153 * month + 2) / 5 + 1;	/* [1, 31] */
	month += month < 10 ? 2 : -10;
	year = ADJUSTED_EPOCH_YEAR + erayear + era * YEARS_PER_ERA + (month <= 1);

	res->tm_yday = yearday >= DAYS_PER_YEAR - DAYS_IN_JANUARY - DAYS_IN_FEBRUARY ?
				   yearday - (DAYS_PER_YEAR - DAYS_IN_JANUARY - DAYS_IN_FEBRUARY) :
				   yearday + DAYS_IN_JANUARY + DAYS_IN_FEBRUARY + isleap(erayear);
	res->tm_year = year - YEAR_BASE;
	res->tm_mon = month;
	res->tm_mday = day;

	res->tm_isdst = 0;

	return (res);
}

struct tm *__wrap_localtime(const time_t *tim_p)
{
	struct _reent *reent = _REENT;

	_REENT_CHECK_TM(reent);
	return gmtime64_r(tim_p, (struct tm *)_REENT_TM(reent));
}

static void
validate_structure(struct tm *tim_p)
{
	div_t res;
	int days_in_feb = 28;

	/* calculate time & date to account for out of range values */
	if (tim_p->tm_sec < 0 || tim_p->tm_sec > 59) {
		res = div(tim_p->tm_sec, 60);
		tim_p->tm_min += res.quot;
		if ((tim_p->tm_sec = res.rem) < 0) {
			tim_p->tm_sec += 60;
			--tim_p->tm_min;
		}
	}

	if (tim_p->tm_min < 0 || tim_p->tm_min > 59) {
		res = div(tim_p->tm_min, 60);
		tim_p->tm_hour += res.quot;
		if ((tim_p->tm_min = res.rem) < 0) {
			tim_p->tm_min += 60;
			--tim_p->tm_hour;
		}
	}

	if (tim_p->tm_hour < 0 || tim_p->tm_hour > 23) {
		res = div(tim_p->tm_hour, 24);
		tim_p->tm_mday += res.quot;
		if ((tim_p->tm_hour = res.rem) < 0) {
			tim_p->tm_hour += 24;
			--tim_p->tm_mday;
		}
	}

	if (tim_p->tm_mon < 0 || tim_p->tm_mon > 11) {
		res = div(tim_p->tm_mon, 12);
		tim_p->tm_year += res.quot;
		if ((tim_p->tm_mon = res.rem) < 0) {
			tim_p->tm_mon += 12;
			--tim_p->tm_year;
		}
	}

	if (_DAYS_IN_YEAR(tim_p->tm_year) == 366) {
		days_in_feb = 29;
	}

	if (tim_p->tm_mday <= 0) {
		while (tim_p->tm_mday <= 0) {
			if (--tim_p->tm_mon == -1) {
				tim_p->tm_year--;
				tim_p->tm_mon = 11;
				days_in_feb =
					((_DAYS_IN_YEAR(tim_p->tm_year) == 366) ?
					 29 : 28);
			}
			if (tim_p->tm_mon != -1) { // to pass coverity test
				tim_p->tm_mday += _DAYS_IN_MONTH(tim_p->tm_mon);
			}
		}
	} else {
		while (tim_p->tm_mday > _DAYS_IN_MONTH(tim_p->tm_mon)) {
			tim_p->tm_mday -= _DAYS_IN_MONTH(tim_p->tm_mon);
			if (++tim_p->tm_mon == 12) {
				tim_p->tm_year++;
				tim_p->tm_mon = 0;
				days_in_feb =
					((_DAYS_IN_YEAR(tim_p->tm_year) == 366) ?
					 29 : 28);
			}
		}
	}
}

time_t __wrap_mktime(struct tm *tim_p)
{
	time_t tim = 0;
	long days = 0;
	int year, isdst = 0;
	__tzinfo_type *tz = __gettzinfo();

	/* validate structure */
	validate_structure(tim_p);

	/* compute hours, minutes, seconds */
	tim += tim_p->tm_sec + (tim_p->tm_min * _SEC_IN_MINUTE) +
		   (tim_p->tm_hour * _SEC_IN_HOUR);

	/* compute days in year */
	days += tim_p->tm_mday - 1;
	days += _DAYS_BEFORE_MONTH[tim_p->tm_mon];
	if (tim_p->tm_mon > 1 && _DAYS_IN_YEAR(tim_p->tm_year) == 366) {
		days++;
	}

	/* compute day of the year */
	tim_p->tm_yday = days;

	if (tim_p->tm_year > 10000 || tim_p->tm_year < -10000) {
		return (time_t) -1;
	}

	/* compute days in other years */
	if ((year = tim_p->tm_year) > 70) {
		for (year = 70; year < tim_p->tm_year; year++) {
			days += _DAYS_IN_YEAR(year);
		}
	} else if (year < 70) {
		for (year = 69; year > tim_p->tm_year; year--) {
			days -= _DAYS_IN_YEAR(year);
		}
		days -= _DAYS_IN_YEAR(year);
	}

	/* compute total seconds */
	tim += (time_t)days * _SEC_IN_DAY;

	/* add appropriate offset to put time in gmt format */
	if (isdst == 1) {
		tim += (time_t) tz->__tzrule[1].offset;
	} else { /* otherwise assume std time */
		tim += (time_t) tz->__tzrule[0].offset;
	}

	/* reset isdst flag to what we have calculated */
	tim_p->tm_isdst = isdst;

	/* compute day of the week */
	if ((tim_p->tm_wday = (days + 4) % 7) < 0) {
		tim_p->tm_wday += 7;
	}

	return tim;
}

char *__wrap_ctime(long long *t)
{
	/* The C Standard macrsays ctime (t) is equivalent to asctime (localtime (t)).
	   In particular, ctime and asctime must yield the same pointer.  */
	return asctime(__wrap_localtime(t));
}
#endif

#endif