#ifndef HEADER_TIMEB_H
#define HEADER_TIMEB_H

struct timeb
{
    time_t time;
    unsigned short millitm;
    short timezone;
    short dstflag;
};

int ftime(struct timeb *t);

size_t strftime(char* ptr, size_t maxsize, const char* format, const struct tm* timeptr);

#endif