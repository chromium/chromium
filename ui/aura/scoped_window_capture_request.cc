// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/scoped_window_capture_request.h"

#include "ui/aura/window.h"

namespace aura {

ScopedWindowCaptureRequest::ScopedWindowCaptureRequest(
    ScopedWindowCaptureRequest&& other)
    : window_(other.window_) {
  other.window_ = nullptr;
}

ScopedWindowCaptureRequest& ScopedWindowCaptureRequest::operator=(
    ScopedWindowCaptureRequest&& rhs) {
  if (window_)
    window_->OnScopedWindowCaptureRequestRemoved();
  window_ = rhs.window_;
  rhs.window_ = nullptr;
  return *this;
}

ScopedWindowCaptureRequest::~ScopedWindowCaptureRequest() {
  if (window_)
    window_->OnScopedWindowCaptureRequestRemoved();
}

viz::SubtreeCaptureId ScopedWindowCaptureRequest::GetCaptureId() const {
  return window_ ? window_->subtree_capture_id() : viz::SubtreeCaptureId();
}

ScopedWindowCaptureRequest::ScopedWindowCaptureRequest(Window* window)
    : window_(window) {
  DCHECK(window_);
  DCHECK(!window_->IsRootWindow());
  window_->OnScopedWindowCaptureRequestAdded();
}

}  // namespace aura
