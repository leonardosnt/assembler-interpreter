#ifndef __PCH_H__
#define __PCH_H__

//#define INC_WINDOWS

#include <stdlib.h>
#include <stdio.h>
#ifdef INC_WINDOWS
  #include <windows.h>
#endif

#ifdef INC_WINDOWS
  #define PERF_START(name) \
    LARGE_INTEGER t1__##name, t2__##name, frequency__##name; \
    double took__##name; \
    QueryPerformanceFrequency(&frequency__##name);  \
    QueryPerformanceCounter(&t1__##name); \


  #define PERF_STOP(name) \
    QueryPerformanceCounter(&t2__##name); \
    took__##name = (float) (t2__##name.QuadPart - t1__##name.QuadPart) / frequency__##name.QuadPart;  \
    printf("%s: %lfms (%lfs)\n", #name, (took__##name * 1000.0), took__##name);
#else
  #define PERF_START(name) /*name*/
  #define PERF_STOP(name) /*name*/
#endif

#endif