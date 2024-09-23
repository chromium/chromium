// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/angle_platform_impl.h"

#include "base/base64.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/trace_event/trace_event.h"
#include "third_party/angle/include/platform/PlatformMethods.h"
#include "ui/gl/gl_bindings.h"

namespace angle {

namespace {

ResetDisplayPlatformFunc g_angle_reset_platform = nullptr;

double ANGLEPlatformImpl_currentTime(PlatformMethods* platform) {
  return base::Time::Now().InSecondsFSinceUnixEpoch();
}

double ANGLEPlatformImpl_monotonicallyIncreasingTime(
    PlatformMethods* platform) {
  return (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
}

const unsigned char* ANGLEPlatformImpl_getTraceCategoryEnabledFlag(
    PlatformMethods* platform,
    const char* category_group) {
  return TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(category_group);
}

void ANGLEPlatformImpl_logError(PlatformMethods* platform,
                                const char* errorMessage) {
  LOG(ERROR) << errorMessage;
}

void ANGLEPlatformImpl_logWarning(PlatformMethods* platform,
                                  const char* warningMessage) {
  LOG(WARNING) << warningMessage;
}

TraceEventHandle ANGLEPlatformImpl_addTraceEvent(
    PlatformMethods* platform,
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    unsigned long long id,
    double timestamp,
    int num_args,
    const char** arg_names,
    const unsigned char* arg_types,
    const unsigned long long* arg_values,
    unsigned char flags) {
  base::TimeTicks timestamp_tt = base::TimeTicks() + base::Seconds(timestamp);
  base::trace_event::TraceArguments args(num_args, arg_names, arg_types,
                                         arg_values);
  base::trace_event::TraceEventHandle handle =
      TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_THREAD_ID_AND_TIMESTAMP(
          phase, category_group_enabled, name,
          trace_event_internal::kGlobalScope, id, trace_event_internal::kNoId,
          base::PlatformThread::CurrentId(), timestamp_tt, &args, flags);
  TraceEventHandle result;
  memcpy(&result, &handle, sizeof(result));
  return result;
}

void ANGLEPlatformImpl_updateTraceEventDuration(
    PlatformMethods* platform,
    const unsigned char* category_group_enabled,
    const char* name,
    TraceEventHandle handle) {
  base::trace_event::TraceEventHandle trace_event_handle;
  memcpy(&trace_event_handle, &handle, sizeof(handle));
  TRACE_EVENT_API_UPDATE_TRACE_EVENT_DURATION(category_group_enabled, name,
                                              trace_event_handle);
}

void ANGLEPlatformImpl_histogramCustomCounts(PlatformMethods* platform,
                                             const char* name,
                                             int sample,
                                             int min,
                                             int max,
                                             int bucket_count) {
  // Copied from histogram macro, but without the static variable caching
  // the histogram because name is dynamic.
  base::HistogramBase* counter = base::Histogram::FactoryGet(
      name, min, max, bucket_count,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->Add(sample);
}

void ANGLEPlatformImpl_histogramEnumeration(PlatformMethods* platform,
                                            const char* name,
                                            int sample,
                                            int boundary_value) {
  // Copied from histogram macro, but without the static variable caching
  // the histogram because name is dynamic.
  base::HistogramBase* counter = base::LinearHistogram::FactoryGet(
      name, 1, boundary_value, boundary_value + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->Add(sample);
}

void ANGLEPlatformImpl_histogramSparse(PlatformMethods* platform,
                                       const char* name,
                                       int sample) {
  base::UmaHistogramSparse(name, sample);
}

void ANGLEPlatformImpl_histogramBoolean(PlatformMethods* platform,
                                        const char* name,
                                        bool sample) {
  ANGLEPlatformImpl_histogramEnumeration(platform, name, sample ? 1 : 0, 2);
}

NO_SANITIZE("cfi-icall")
void AnglePlatformImpl_runWorkerTask(PostWorkerTaskCallback callback, void* user_data) {
  TRACE_EVENT0("toplevel", "ANGLEPlatformImpl::RunWorkerTask");
  callback(user_data);
}

void ANGLEPlatformImpl_postWorkerTask(PlatformMethods* platform,
                                      PostWorkerTaskCallback callback,
                                      void* user_data) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&AnglePlatformImpl_runWorkerTask, callback, user_data));
}

int g_cache_hit_count = 0;
int g_cache_miss_count = 0;

base::Lock& GetCacheStatsLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

void RecordCacheUse() {
  base::AutoLock lock(GetCacheStatsLock());
  base::UmaHistogramCounts100("GPU.ANGLE.MetalShader.CacheHitCount",
                              g_cache_hit_count);
  base::UmaHistogramCounts100("GPU.ANGLE.MetalShader.CacheMissCount",
                              g_cache_miss_count);
}

void ANGLEPlatformImpl_recordShaderCacheUse(bool in_cache) {
  static bool did_schedule_log = false;
  bool post_task = false;
  {
    base::AutoLock lock(GetCacheStatsLock());
    if (!did_schedule_log) {
      did_schedule_log = true;
      post_task = true;
    }
    if (in_cache) {
      ++g_cache_hit_count;
    } else {
      ++g_cache_miss_count;
    }
  }
  if (post_task) {
    // Record the stats soonish after the first call. Ideally this would be
    // logged along with startup, but that's rather complex to determine from
    // here (as well as pluming through to browser side).
    // The 90 seconds comes from the 99 percentile of startup time on macos.
    base::ThreadPool::PostDelayedTask(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&RecordCacheUse), base::Seconds(90));
  }
}

}  // anonymous namespace

NO_SANITIZE("cfi-icall")
bool InitializePlatform(EGLDisplay display) {
  GetDisplayPlatformFunc angle_get_platform =
      reinterpret_cast<GetDisplayPlatformFunc>(
          eglGetProcAddress("ANGLEGetDisplayPlatform"));
  if (!angle_get_platform)
    return false;

  // Save the pointer to the destroy function here to avoid crash.
  g_angle_reset_platform = reinterpret_cast<ResetDisplayPlatformFunc>(
      eglGetProcAddress("ANGLEResetDisplayPlatform"));

  PlatformMethods* platformMethods = nullptr;
  if (!angle_get_platform(static_cast<EGLDisplayType>(display),
                          g_PlatformMethodNames, g_NumPlatformMethods, nullptr,
                          &platformMethods))
    return false;
  platformMethods->currentTime = ANGLEPlatformImpl_currentTime;
  platformMethods->addTraceEvent = ANGLEPlatformImpl_addTraceEvent;
  platformMethods->getTraceCategoryEnabledFlag =
      ANGLEPlatformImpl_getTraceCategoryEnabledFlag;
  platformMethods->histogramBoolean = ANGLEPlatformImpl_histogramBoolean;
  platformMethods->histogramCustomCounts =
      ANGLEPlatformImpl_histogramCustomCounts;
  platformMethods->histogramEnumeration =
      ANGLEPlatformImpl_histogramEnumeration;
  platformMethods->histogramSparse = ANGLEPlatformImpl_histogramSparse;
  platformMethods->logError = ANGLEPlatformImpl_logError;
  platformMethods->logWarning = ANGLEPlatformImpl_logWarning;
  platformMethods->monotonicallyIncreasingTime =
      ANGLEPlatformImpl_monotonicallyIncreasingTime;
  platformMethods->updateTraceEventDuration =
      ANGLEPlatformImpl_updateTraceEventDuration;
  platformMethods->recordShaderCacheUse =
      ANGLEPlatformImpl_recordShaderCacheUse;

  // Initialize the delegate to allow posting tasks in the Chromium thread pool.
  // The thread pool is not available in some unittests.
  if (base::ThreadPoolInstance::Get())
    platformMethods->postWorkerTask = ANGLEPlatformImpl_postWorkerTask;
  return true;
}

NO_SANITIZE("cfi-icall")
void ResetPlatform(EGLDisplay display) {
  if (!g_angle_reset_platform)
    return;
  g_angle_reset_platform(static_cast<EGLDisplayType>(display));
}

}  // namespace angle
