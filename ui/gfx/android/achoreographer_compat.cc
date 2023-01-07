// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/android/achoreographer_compat.h"

#include <dlfcn.h>

#include "base/android/build_info.h"
#include "base/logging.h"

#define LOAD_FUNCTION(lib, func)                             \
  do {                                                       \
    func##Fn = reinterpret_cast<p##func>(dlsym(lib, #func)); \
    if (!func##Fn) {                                         \
      supported = false;                                     \
      LOG(ERROR) << "Unable to load function " << #func;     \
    }                                                        \
  } while (0)

namespace gfx {

// static
const AChoreographerCompat& AChoreographerCompat::Get() {
  static AChoreographerCompat instance;
  return instance;
}

AChoreographerCompat::AChoreographerCompat() {
  void* main_dl_handle = dlopen("libandroid.so", RTLD_NOW);
  if (!main_dl_handle) {
    LOG(ERROR) << "Couldnt load libandroid.so";
    supported = false;
    return;
  }

  LOAD_FUNCTION(main_dl_handle, AChoreographer_getInstance);
  LOAD_FUNCTION(main_dl_handle, AChoreographer_postFrameCallback64);
  LOAD_FUNCTION(main_dl_handle, AChoreographer_registerRefreshRateCallback);
  LOAD_FUNCTION(main_dl_handle, AChoreographer_unregisterRefreshRateCallback);
}

// static
const AChoreographerCompat33& AChoreographerCompat33::Get() {
  static AChoreographerCompat33 instance;
  return instance;
}

AChoreographerCompat33::AChoreographerCompat33() {
  if (!base::android::BuildInfo::GetInstance()->is_at_least_t()) {
    supported = false;
    return;
  }

  void* main_dl_handle = dlopen("libandroid.so", RTLD_NOW);
  if (!main_dl_handle) {
    LOG(ERROR) << "Couldnt load libandroid.so";
    supported = false;
    return;
  }

  LOAD_FUNCTION(main_dl_handle, AChoreographer_postVsyncCallback);
  LOAD_FUNCTION(main_dl_handle,
                AChoreographerFrameCallbackData_getFrameTimeNanos);
  LOAD_FUNCTION(main_dl_handle,
                AChoreographerFrameCallbackData_getFrameTimelinesLength);
  LOAD_FUNCTION(main_dl_handle,
                AChoreographerFrameCallbackData_getPreferredFrameTimelineIndex);
  LOAD_FUNCTION(main_dl_handle,
                AChoreographerFrameCallbackData_getFrameTimelineVsyncId);
  LOAD_FUNCTION(
      main_dl_handle,
      AChoreographerFrameCallbackData_getFrameTimelineExpectedPresentationTimeNanos);
  LOAD_FUNCTION(main_dl_handle,
                AChoreographerFrameCallbackData_getFrameTimelineDeadlineNanos);
}

}  // namespace gfx
