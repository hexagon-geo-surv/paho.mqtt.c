#include <winsock2.h>
#include "sys/timeb.h"

#include "time.h"

int ftime(struct timeb *t)
{
    SYSTEMTIME systime;
    FILETIME filetime;
    __int64 ft;
    static __int64 offsetEpoch = 116444736000000000; // 100ns-ticks from 1.1.1601 to 1.1.1970 (->offset into our epoch)
    
    GetLocalTime(&systime);
    t->dstflag = 0;
    t->timezone = 0;
    t->millitm = systime.wMilliseconds; 

    SystemTimeToFileTime(&systime, &filetime);
	
    ft = filetime.dwHighDateTime;
    ft <<= 32;
    ft |= filetime.dwLowDateTime;
	
    ft -= offsetEpoch;
    t->time = (time_t)(ft / 10000000);
	
    return 0;
}

size_t strftime(char* ptr, size_t maxsize, const char* format, const struct tm* timeptr)
{
	return 0;
}
