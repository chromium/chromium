// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/angle_platform_impl.h"

#include <atomic>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/trace_event/trace_event.h"
#include "third_party/angle/include/platform/PlatformMethods.h"
#include "ui/gl/gl_bindings.h"

namespace angle {

namespace {

std::atomic<bool> g_post_task_failed_for_testing{false};

double ANGLEPlatformImpl_currentTime(PlatformMethods* platform) {
  return base::Time::Now().InSecondsFSinceUnixEpoch();
}

double ANGLEPlatformImpl_monotonicallyIncreasingTime(
    PlatformMethods* platform) {
  return (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
}

void ANGLEPlatformImpl_logError(PlatformMethods* platform,
                                const char* errorMessage) {
  LOG(ERROR) << errorMessage;
}

void ANGLEPlatformImpl_logWarning(PlatformMethods* platform,
                                  const char* warningMessage) {
  LOG(WARNING) << warningMessage;
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

void ANGLEPlatformImpl_recordShaderCacheUse(bool in_cache) {
  // Metrics were no longer required, we can remove once Angle no longer
  // requires the method.
}

}  // anonymous namespace

void SetPostTaskFailedForTesting(bool failed) {
  g_post_task_failed_for_testing.store(failed);
}

void ANGLEPlatformImpl_postWorkerTask(PlatformMethods* platform,
                                      PostWorkerTaskCallback callback,
                                      void* user_data) {
  bool success = false;
  if (!g_post_task_failed_for_testing.load()) {
    success = base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::USER_BLOCKING},
        base::BindOnce(&AnglePlatformImpl_runWorkerTask, callback, user_data));
  }
  if (!success) {
    // Run the task synchronously if posting failed (e.g. during shutdown) to
    // avoid GPU hangs. See https://crbug.com/539435331
    AnglePlatformImpl_runWorkerTask(callback, user_data);
  }
}

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
