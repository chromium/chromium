// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mac/display_link_mac.h"

#include "base/feature_list.h"
#include "base/task/bind_post_task.h"
#include "ui/display/mac/ca_display_link_mac.h"
#include "ui/display/mac/cv_display_link_mac.h"

namespace ui {

BASE_FEATURE(kCADisplayLink,
             "CADisplayLink",
             base::FEATURE_DISABLED_BY_DEFAULT);

////////////////////////////////////////////////////////////////////////////////
// DisplayLinkMac

// static
scoped_refptr<DisplayLinkMac> DisplayLinkMac::GetForDisplay(
    int64_t vsync_display_id) {
  if (!vsync_display_id) {
    return nullptr;
  }

  CGDirectDisplayID display_id =
      base::checked_cast<CGDirectDisplayID>(vsync_display_id);

  if (base::FeatureList::IsEnabled(kCADisplayLink)) {
    if (@available(macos 14.0, *)) {
      // CADisplayLink is available only for MacOS 14.0+.
      return CADisplayLinkMac::GetForDisplayOnCurrentThread(display_id);
    }
  }

  // CADisplayLink is available for MacOS 10.4–15.0.
  return CVDisplayLinkMac::GetForDisplay(display_id);
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

}  // namespace ui
