// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTS_TIMING_H
#define TESTS_TIMING_H

#include <assert.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/time.h>
#endif
#include <time.h>

#if defined(_WIN32)

static double seconds()
{
    static double clock_frequency;
    static bool have_frequency;

    LARGE_INTEGER qpc;
    QueryPerformanceCounter(&qpc);
    if (have_frequency)
        return qpc.QuadPart * clock_frequency;

    have_frequency = true;
    QueryPerformanceFrequency(&qpc);
    clock_frequency = 1.0 / (double) qpc.QuadPart;
    return seconds();
}

#else

static double seconds()
{
    struct timeval now;
    gettimeofday(&now, 0);
    return now.tv_sec + now.tv_usec * (1.0 / 1000000.0);
}

#endif

#define TIME(function, time) do {  \
    double start = seconds();      \
    (function);                    \
    *time += seconds() - start;    \
} while (0)

#endif // TESTS_TIMING_H
