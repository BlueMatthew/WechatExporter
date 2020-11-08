#ifndef TIME64_H
#    define TIME64_H

#include <time.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* Set our custom types */
typedef long long       Int64;
typedef Int64           Time64_T;
typedef Int64           Year;


/* A copy of the tm struct but with a 64 bit year */
struct TM64 {
        int     tm_sec;
        int     tm_min;
        int     tm_hour;
        int     tm_mday;
        int     tm_mon;
        Year    tm_year;
        int     tm_wday;
        int     tm_yday;
        int     tm_isdst;

#ifdef HAVE_TM_TM_GMTOFF
        long    tm_gmtoff;
#endif

#ifdef HAVE_TM_TM_ZONE
        char    *tm_zone;
#endif
};


/* Decide which tm struct to use */
#ifdef USE_TM64
#define TM      TM64
#else
#define TM      tm
#endif


/* Declare public functions */
struct TM *gmtime64_r    (const Time64_T *, struct TM *);
struct TM *localtime64_r (const Time64_T *, struct TM *);

char *asctime64_r        (const struct TM *, char *);

char *ctime64_r          (const Time64_T*, char*);

Time64_T   timegm64      (const struct TM *);
Time64_T   mktime64      (struct TM *);
Time64_T   timelocal64   (struct TM *);


/* Not everyone has gm/localtime_r(), provide a replacement */
#ifdef HAVE_LOCALTIME_R
#    define LOCALTIME_R(clock, result) localtime_r(clock, result)
#else
#    define LOCALTIME_R(clock, result) fake_localtime_r(clock, result)
#endif
#ifdef HAVE_GMTIME_R
#    define GMTIME_R(clock, result)    gmtime_r(clock, result)
#else
#    define GMTIME_R(clock, result)    fake_gmtime_r(clock, result)
#endif


/* Use a different asctime format depending on how big the year is */
#ifdef USE_TM64
    #define TM64_ASCTIME_FORMAT "%.3s %.3s%3d %.2d:%.2d:%.2d %lld\n"
#else
    #define TM64_ASCTIME_FORMAT "%.3s %.3s%3d %.2d:%.2d:%.2d %d\n"
#endif

void copy_tm_to_TM64(const struct tm *src, struct TM *dest);
void copy_TM64_to_tm(const struct TM *src, struct tm *dest);

#endif
