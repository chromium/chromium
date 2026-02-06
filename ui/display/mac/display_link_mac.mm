// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mac/display_link_mac.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/bind_post_task.h"
#include "ui/display/display_features.h"
#include "ui/display/mac/ca_display_link_mac.h"
#include "ui/display/mac/cv_display_link_mac.h"
#include "ui/display/mac/external_display_link_mac.h"
#include "ui/display/types/display_constants.h"

namespace ui {

// For testing only. Create CADisplayLink in the GPU process.
BASE_FEATURE(kCADisplayLinkinGpu, base::FEATURE_DISABLED_BY_DEFAULT);

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

// Static
bool DisplayLinkMac::IsDisplayLinkAllowed(int64_t display_id) {
  if (DisplayLinkMac::SupportsDisplayLinkMacInBrowser()) {
    return ExternalDisplayLinkMac::IsDisplayLinkSupported(display_id);
  }

  return true;
}

// static
scoped_refptr<DisplayLinkMac> DisplayLinkMac::GetForDisplay(
    int64_t vsync_display_id) {
  if (vsync_display_id == display::kInvalidDisplayId) {
    return nullptr;
  }

  CGDirectDisplayID display_id =
      base::checked_cast<CGDirectDisplayID>(vsync_display_id);

  // CADisplayLink is available only for MacOS 14.0+.
  if (@available(macos 14.0, *)) {
    if (base::FeatureList::IsEnabled(kCADisplayLinkinGpu)) {
      return CADisplayLinkMac::GetForDisplay(display_id);
    }
  }

  if (SupportsDisplayLinkMacInBrowser()) {
    return ExternalDisplayLinkMac::GetForDisplay(display_id);
  }

  return CVDisplayLinkMac::GetForDisplay(display_id);
}

void DisplayLinkMac::RecordDisplayLinkCreation(bool success) {
  UMA_HISTOGRAM_BOOLEAN("Viz.ExternalBeginFrameSourceMac.DisplayLink.Create",
                        success);
}

std::unique_ptr<PresentationCallbackMac>
DisplayLinkMac::RegisterPresentationCallback(
    PresentationCallbackMac::Callback callback) {
  NOTREACHED();
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
