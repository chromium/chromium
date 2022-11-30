// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANDROID_ACHOREOGRAPHER_COMPAT_H_
#define UI_GFX_ANDROID_ACHOREOGRAPHER_COMPAT_H_

#include <sys/types.h>

#include "ui/gfx/gfx_export.h"

extern "C" {
typedef struct AChoreographer AChoreographer;
typedef void (*AChoreographer_frameCallback64)(int64_t, void*);
typedef void (*AChoreographer_refreshRateCallback)(int64_t, void*);

using pAChoreographer_getInstance = AChoreographer* (*)();
using pAChoreographer_postFrameCallback64 =
    void (*)(AChoreographer*, AChoreographer_frameCallback64, void*);
using pAChoreographer_registerRefreshRateCallback =
    void (*)(AChoreographer*, AChoreographer_refreshRateCallback, void*);
using pAChoreographer_unregisterRefreshRateCallback =
    void (*)(AChoreographer*, AChoreographer_refreshRateCallback, void*);

typedef struct AChoreographerFrameCallbackData AChoreographerFrameCallbackData;
typedef void (*AChoreographer_vsyncCallback)(
    const AChoreographerFrameCallbackData*,
    void*);

using pAChoreographer_postVsyncCallback =
    void (*)(AChoreographer* choreographer,
             AChoreographer_vsyncCallback callback,
             void* data);
using pAChoreographerFrameCallbackData_getFrameTimeNanos =
    int64_t (*)(const AChoreographerFrameCallbackData*);
using pAChoreographerFrameCallbackData_getFrameTimelinesLength =
    size_t (*)(const AChoreographerFrameCallbackData*);
using pAChoreographerFrameCallbackData_getPreferredFrameTimelineIndex =
    size_t (*)(const AChoreographerFrameCallbackData*);
using pAChoreographerFrameCallbackData_getFrameTimelineVsyncId =
    int64_t (*)(const AChoreographerFrameCallbackData*, size_t);
using pAChoreographerFrameCallbackData_getFrameTimelineExpectedPresentationTimeNanos =
    int64_t (*)(const AChoreographerFrameCallbackData*, size_t);
using pAChoreographerFrameCallbackData_getFrameTimelineDeadlineNanos =
    int64_t (*)(const AChoreographerFrameCallbackData*, size_t);
}  // extern "C"

namespace gfx {

struct GFX_EXPORT AChoreographerCompat {
  static GFX_EXPORT const AChoreographerCompat& Get();

  bool supported = true;
  pAChoreographer_getInstance AChoreographer_getInstanceFn = nullptr;
  pAChoreographer_postFrameCallback64 AChoreographer_postFrameCallback64Fn =
      nullptr;
  pAChoreographer_registerRefreshRateCallback
      AChoreographer_registerRefreshRateCallbackFn = nullptr;
  pAChoreographer_unregisterRefreshRateCallback
      AChoreographer_unregisterRefreshRateCallbackFn = nullptr;

 private:
  AChoreographerCompat();
};

struct GFX_EXPORT AChoreographerCompat33 {
  static GFX_EXPORT const AChoreographerCompat33& Get();

  bool supported = true;
  pAChoreographer_postVsyncCallback AChoreographer_postVsyncCallbackFn =
      nullptr;
  pAChoreographerFrameCallbackData_getFrameTimeNanos
      AChoreographerFrameCallbackData_getFrameTimeNanosFn = nullptr;
  pAChoreographerFrameCallbackData_getFrameTimelinesLength
      AChoreographerFrameCallbackData_getFrameTimelinesLengthFn = nullptr;
  pAChoreographerFrameCallbackData_getPreferredFrameTimelineIndex
      AChoreographerFrameCallbackData_getPreferredFrameTimelineIndexFn =
          nullptr;
  pAChoreographerFrameCallbackData_getFrameTimelineVsyncId
      AChoreographerFrameCallbackData_getFrameTimelineVsyncIdFn = nullptr;
  pAChoreographerFrameCallbackData_getFrameTimelineExpectedPresentationTimeNanos
      AChoreographerFrameCallbackData_getFrameTimelineExpectedPresentationTimeNanosFn =
          nullptr;
  pAChoreographerFrameCallbackData_getFrameTimelineDeadlineNanos
      AChoreographerFrameCallbackData_getFrameTimelineDeadlineNanosFn = nullptr;

 private:
  AChoreographerCompat33();
};

}  // namespace gfx

#endif  // UI_GFX_ANDROID_ACHOREOGRAPHER_COMPAT_H_
