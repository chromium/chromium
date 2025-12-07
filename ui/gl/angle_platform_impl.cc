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
          phase, category_group_enabled, name, id,
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

void ANGLEPlatformImpl_recordShaderCacheUse(bool in_cache) {
  // Metrics were no longer required, we can remove once Angle no longer
  // requires the method.
}

}  // anonymous namespace

NO_SANITIZE("cfi-icall")
bool InitializePlatform(EGLDisplay display,
                        GLGetProcAddressProc get_proc_address) {
  GetDisplayPlatformFunc angle_get_platform =
      reinterpret_cast<GetDisplayPlatformFunc>(
          get_proc_address("ANGLEGetDisplayPlatform"));
  if (!angle_get_platform)
    return false;

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
void ResetPlatform(EGLDisplay display, GLGetProcAddressProc get_proc_address) {
  ResetDisplayPlatformFunc angle_reset_platform =
      reinterpret_cast<ResetDisplayPlatformFunc>(
          get_proc_address("ANGLEResetDisplayPlatform"));
  if (!angle_reset_platform) {
    return;
  }
  angle_reset_platform(static_cast<EGLDisplayType>(display));
}

}  // namespace angle
