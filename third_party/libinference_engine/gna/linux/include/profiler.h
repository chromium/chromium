//*****************************************************************************
// Copyright (C) 2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions
// and limitations under the License.
//
//
// SPDX-License-Identifier: Apache-2.0
//*****************************************************************************

// profiler.h : definitions of profiler
//

#ifndef _PROFILER_H
#define _PROFILER_H

#if defined(__GNUC__)
#include <assert.h>
#endif

#include <sys/timeb.h>
#if defined(_WIN32)
#include <intrin.h>
#else
#include <mmintrin.h>
#endif // os

#if defined(DRIVER) && defined(_WIN32)
#include <wdm.h>
#endif

// enables or disables simple profiling
#if defined(PROFILE) || defined(PROFILE_DETAILED)
#undef PROFILE
#define PROFILE         1
#define PROFILE_(...)   __VA_ARGS__
#else
#define PROFILE_(...)
// disable basic profiling procedures
#define profilerTscStart(...)
#define profilerTscStop(...)
#define profilerTscGetMicros(...) TIME_TSC_MAX
#define profilerRtcStart(...)
#define profilerRtcStop(...)
#define profilerRtcGetMilis(...)  TIME_TSC_MAX
#endif // defined(PROFILE) || defined(PROFILE_DETAILED)

// enables or disables detailed profiling
#if defined(PROFILE) && defined(PROFILE_DETAILED)
#define PROFILE_D_(...) __VA_ARGS__
// enable detailed profiling procedures
#define profilerDTscStart        profilerTscStart
#define profilerDTscStop         profilerTscStop
#define profilerDTscGetMicros    profilerTscGetMicros
#if !defined(DRIVER)
#define profilerDRtcStart        profilerRtcStart
#define profilerDRtcStop         profilerRtcStop
#define profilerDRtcGetMilis     profilerRtcGetMilis
#endif // DRIVER
#else
#define PROFILE_D_(...)
// disable detailed profiling procedures
#define profilerDTscStart(...)
#define profilerDTscStop(...)
#define profilerDTscGetMicros(...) TIME_TSC_MAX
#if !defined(DRIVER)
#define profilerDRtcStart(...)
#define profilerDRtcStop(...)
#define profilerDRtcGetMilis(...)  TIME_TSC_MAX
#endif // DRIVER
#endif // defined(PROFILE) && defined(PROFILE_DETAILED)

// enables or disables profile print macro
#if defined(PROFILE_PRINT)
#define PROFILE_PRINT_      PROFILE_
#define PROFILE_PRINT_D_    PROFILE_D_
#else
#define PROFILE_PRINT_(...)
#define PROFILE_PRINT_D_(...)
#endif // defined(PROFILE_PRINT)

#ifndef PERF_TYPE_DEF

#define PERF_TYPE_DEF

/**
 * max value of time_tsc type
 */
#define TIME_TSC_MAX ULLONG_MAX

/**
 * Time Stamp Counter time type
 */
typedef unsigned long long time_tsc;

static_assert(8 == sizeof(time_tsc), "Invalid size of time_tsc");

#if !defined(DRIVER)
/**
 * Real Time Clock time type
 */
#if defined(_WIN32)
typedef struct __timeb64    time_rtc;
#else
typedef struct timeb    time_rtc;
#endif //os
#endif // DRIVER


#endif //PERF_TYPE_DEF

/**
 * Timestamp counter profiler
 */
typedef struct
{
    time_tsc            start;      // time value on profiler start
    time_tsc            stop;       // time value on profiler stop
} intel_gna_profiler_tsc;

static_assert(16 == sizeof(intel_gna_profiler_tsc), "Invalid size of intel_gna_profiler_tsc");

#if !defined(DRIVER)
/**
 * Realtime clock profiler
 */
typedef struct
{
    time_rtc            start;      // time value on profiler start
    time_rtc            stop;       // time value on profiler stop
    time_rtc            passed;     // time passed between start and stop
} intel_gna_profiler_rtc;
#endif //DRIVER

#if defined(PROFILE) || defined(PROFILE_DETAILED)

#ifdef __cplusplus
extern "C" {  // API uses C linkage so that it can be used by C and C++ applications
#endif

/**
 * Start TSC profiler
 *
 * @p profiler object to start
 */
void profilerTscStart(intel_gna_profiler_tsc* p);

/**
* Stop TSC profiler
*
* @p   profiler object to stop
*/
void profilerTscStop(intel_gna_profiler_tsc* p);

/**
 * Get TSC profiler ticks passed
 */
inline time_tsc profilerGetTscPassed(intel_gna_profiler_tsc const * const profiler)
{
    return profiler->stop - profiler->start;
}

#if !defined(DRIVER)
/**
 * Start RTC profiler
 *
 * NOTE: available resolution is 10-15ms
 *
 * @p profiler object to start
 */
void profilerRtcStart(intel_gna_profiler_rtc* p);

/**
 * Stop RTC profiler
 *
 * NOTE: available resolution is 10-15ms
 *
 * @p   profiler object to stop
 */
void profilerRtcStop(intel_gna_profiler_rtc* p);

/**
 * Get passed miliseconds
 *
 * NOTE: available resolution is 10-15ms
 *
 * @p       stopped profiler object
 * @return  passed time in miliseconds (or TIME_TSC_MAX if p is invalid)
 */
time_tsc profilerRtcGetMilis(intel_gna_profiler_rtc* p);

#endif //DRIVER
#ifdef __cplusplus
}
#endif

#endif //#if defined(PROFILE) || defined(PROFILE_DETAILED)

#endif  // ifndef _PROFILER_H
