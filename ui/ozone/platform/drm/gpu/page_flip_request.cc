// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/page_flip_request.h"

#include <utility>

#include "base/bind.h"
#include "ui/gfx/presentation_feedback.h"

namespace ui {

PageFlipRequest::PageFlipRequest(const base::TimeDelta& refresh_interval)
    : refresh_interval_(refresh_interval) {}

PageFlipRequest::~PageFlipRequest() {
}

void PageFlipRequest::TakeCallback(PresentationOnceCallback callback) {
  DCHECK(!callback_);
  callback_ = std::move(callback);
}

PageFlipRequest::PageFlipCallback PageFlipRequest::AddPageFlip() {
  ++page_flip_count_;
  return base::BindOnce(&PageFlipRequest::Signal, this);
}

void PageFlipRequest::Signal(unsigned int frame, base::TimeTicks timestamp) {
  if (--page_flip_count_ != 0 || callback_.is_null())
    return;

  // For Ozone DRM, the page flip is aligned with VSYNC, and the timestamp is
  // provided by kernel DRM driver (kHWClock) and the buffer has been presented
  // on the screen (kHWCompletion).
  const uint32_t kFlags = gfx::PresentationFeedback::Flags::kVSync |
                          gfx::PresentationFeedback::Flags::kHWClock |
                          gfx::PresentationFeedback::Flags::kHWCompletion;
  gfx::PresentationFeedback feedback(timestamp, refresh_interval_, kFlags);
  std::move(callback_).Run(feedback);
}

}  // namespace ui
