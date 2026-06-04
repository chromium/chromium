// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mac/display_link_mac.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/bind_post_task.h"
#include "ui/display/display_features.h"
#include "ui/display/mac/ca_display_link_mac.h"
#include "ui/display/mac/cv_display_link_mac.h"
#include "ui/display/mac/external_display_link_mac.h"
#include "ui/display/mac/screen_utils_mac.h"
#include "ui/display/types/display_constants.h"

namespace ui {

// For testing only. Create CADisplayLink in the GPU process.
BASE_FEATURE(kCADisplayLinkInGpu, base::FEATURE_DISABLED_BY_DEFAULT);

////////////////////////////////////////////////////////////////////////////////
// DisplayLinkMac

// Static
bool DisplayLinkMac::SupportsDisplayLinkMacInBrowser() {
  if (!@available(macos 14.0, *)) {
    return false;
  }

  return base::FeatureList::IsEnabled(
      display::features::kCADisplayLinkInBrowser);
}

// static
scoped_refptr<DisplayLinkMac> DisplayLinkMac::GetForDisplay(
    int64_t vsync_display_id) {
  // Convert int64_t to uint32_t. A negative id is an internal constant such as
  // display::kInvalidDisplayId, which is invalid for creating a CADisplayLink.
  // CGDirectDisplayID has a type of uint32_t.
  if (!base::IsValueInRangeForNumericType<CGDirectDisplayID>(
          vsync_display_id)) {
    return nullptr;
  }

  CGDirectDisplayID display_id =
      static_cast<CGDirectDisplayID>(vsync_display_id);

  // CADisplayLink is available only for MacOS 14.0+.
  if (@available(macos 14.0, *)) {
    // Testing only.
    if (base::FeatureList::IsEnabled(kCADisplayLinkInGpu)) {
      return CADisplayLinkMac::GetForDisplay(display_id,
                                             /*in_gpu_process=*/true);
    }
  }

  scoped_refptr<DisplayLinkMac> display_link;
  if (SupportsDisplayLinkMacInBrowser()) {
    if (CADisplayLinkMac::IsValidInGpuProcess(display_id)) {
      // Start with CADisplayLinkMac in the GPU process.
      display_link = CADisplayLinkMac::GetForDisplay(display_id,
                                                     /*in_gpu_process=*/true);
      if (display_link) {
        return display_link;
      }
      // Fallback to ExternalDisplayLinkMac (CADisplayLinkMac in the Browser
      // process) if failed.
    }

    display_link = ExternalDisplayLinkMac::GetForDisplay(display_id);
    if (display_link) {
      return display_link;
    }
    // Fallback to CVDisplayLinkMac if failed.
  }

  return CVDisplayLinkMac::GetForDisplay(display_id);
}

void DisplayLinkMac::RecordDisplayLinkCreation(bool success) {
  UMA_HISTOGRAM_BOOLEAN("Viz.ExternalBeginFrameSourceMac.DisplayLink.Create2",
                        success);
}

// static
base::TimeDelta DisplayLinkMac::GetScreenDefaultRefreshInterval(
    int64_t vsync_display_id) {
  if (!base::IsValueInRangeForNumericType<CGDirectDisplayID>(
          vsync_display_id)) {
    return base::Seconds(1) / 60.0;
  }

  CGDirectDisplayID display_id =
      static_cast<CGDirectDisplayID>(vsync_display_id);
  return display::GetNSScreenRefreshInterval(display_id);
}

std::unique_ptr<PresentationCallbackMac>
DisplayLinkMac::RegisterPresentationCallback(
    PresentationCallbackMac::Callback callback) {
  NOTREACHED();
}

bool DisplayLinkMac::NotifyEventAndCheckValidity() {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// VSyncCallbackMac

VSyncCallbackMac::VSyncCallbackMac(UnregisterCallback unregister_callback,
                                   Callback callback,
                                   bool post_callback_to_ctor_thread)
    : unregister_callback_(std::move(unregister_callback)) {
  if (post_callback_to_ctor_thread) {
    auto lambda = [](base::WeakPtr<VSyncCallbackMac> weak_this,
                     Callback callback, VSyncParamsMac params) {
      if (weak_this) {
        callback.Run(params);
      }
    };
    auto callback_for_current_thread =
        base::BindRepeating(lambda, weak_factory_.GetWeakPtr(), callback);
    callback_for_displaylink_thread_ =
        base::BindPostTaskToCurrentDefault(callback_for_current_thread);
  } else {
    callback_for_displaylink_thread_ = std::move(callback);
  }
}

VSyncCallbackMac::~VSyncCallbackMac() {
  std::move(unregister_callback_).Run(this);
}

base::WeakPtr<VSyncCallbackMac> VSyncCallbackMac::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

////////////////////////////////////////////////////////////////////////////////
// PresentationCallbackMac

PresentationCallbackMac::PresentationCallbackMac(
    UnregisterCallback unregister_callback,
    Callback callback,
    bool post_callback_to_ctor_thread)
    : unregister_callback_(std::move(unregister_callback)) {
  if (post_callback_to_ctor_thread) {
    auto lambda = [](base::WeakPtr<PresentationCallbackMac> weak_this,
                     Callback callback, int64_t drawable_id,
                     base::TimeTicks time) {
      if (weak_this) {
        callback.Run(drawable_id, time);
      }
    };
    auto callback_for_current_thread =
        base::BindRepeating(lambda, weak_factory_.GetWeakPtr(), callback);
    callback_for_displaylink_thread_ =
        base::BindPostTaskToCurrentDefault(callback_for_current_thread);
  } else {
    callback_for_displaylink_thread_ = std::move(callback);
  }
}

PresentationCallbackMac::~PresentationCallbackMac() {
  std::move(unregister_callback_).Run(this);
}

base::WeakPtr<PresentationCallbackMac> PresentationCallbackMac::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace ui
