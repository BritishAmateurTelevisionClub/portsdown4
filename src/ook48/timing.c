#include <time.h>
#include <errno.h>

#include "timing.h"

uint64_t monotonic_ms(void)
{
    struct timespec tp;

    if(clock_gettime(CLOCK_MONOTONIC, &tp) != 0)
    {
        return 0;
    }

    return (uint64_t) tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
}

uint64_t timestamp_ms(void)
{
    struct timespec tp;

    if(clock_gettime(CLOCK_REALTIME, &tp) != 0)
    {
        return 0;
    }

    return (uint64_t) tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
}

void sleep_ms(uint32_t _duration)
{
    struct timespec req, rem;
    req.tv_sec = _duration / 1000;
    req.tv_nsec = (_duration - (req.tv_sec*1000))*1000*1000;

    while(nanosleep(&req, &rem) == EINTR)
    {
        /* Interrupted by signal, shallow copy remaining time into request, and resume */
        req = rem;
    }
}

