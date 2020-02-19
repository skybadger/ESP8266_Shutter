#if ! defined PROJDEFS_H
#define PROJDEFS_H

#define TZ              0       // (utc+) TZ in hours
#define DST_MN          60      // use 60mn for summer time in some countries
#define TZ_MN           ((TZ)*60)
#define TZ_SEC          ((TZ)*3600)
#define DST_SEC         ((DST_MN)*60)

#define _TESTING

#define MAX_STRING_LENGTH 25

#endif
