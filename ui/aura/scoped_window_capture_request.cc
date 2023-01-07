// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/scoped_window_capture_request.h"

#include "ui/aura/window.h"

namespace aura {

ScopedWindowCaptureRequest::ScopedWindowCaptureRequest(
    ScopedWindowCaptureRequest&& other)
    // Do not decrement requests on |other| nor increment them on |this| since
    // we are moving the same request into here.
    : window_(other.DetachFromCurrentWindow(/*decrement_requests=*/false)) {
  if (window_)
    AttachToCurrentWindow(/*increment_requests=*/false);
}

ScopedWindowCaptureRequest& ScopedWindowCaptureRequest::operator=(
    ScopedWindowCaptureRequest&& rhs) {
  // Note that |this| might have been attached to a different window than that
  // of |rhs|, so we need to detach from while decrementing the requests.
  DetachFromCurrentWindow(/*decrement_requests=*/true);

  // However, |rhs| is moving into |this|, so it's essentially the same request,
  // therefore, no need to either increment or decrement the requests.
  window_ = rhs.DetachFromCurrentWindow(/*decrement_requests=*/false);
  if (window_)
    AttachToCurrentWindow(/*increment_requests=*/false);

  return *this;
}

ScopedWindowCaptureRequest::~ScopedWindowCaptureRequest() {
  DetachFromCurrentWindow(/*decrement_requests=*/true);
}

viz::SubtreeCaptureId ScopedWindowCaptureRequest::GetCaptureId() const {
  return window_ ? window_->subtree_capture_id() : viz::SubtreeCaptureId();
}

void ScopedWindowCaptureRequest::OnWindowDestroying(Window* window) {
  // No need to call OnScopedWindowCaptureRequestRemoved() since the window is
  // being destroyed.
  DetachFromCurrentWindow(/*decrement_requests=*/false);
}

ScopedWindowCaptureRequest::ScopedWindowCaptureRequest(Window* window)
    : window_(window) {
  AttachToCurrentWindow(/*increment_requests=*/true);
}

void ScopedWindowCaptureRequest::AttachToCurrentWindow(
    bool increment_requests) {
  DCHECK(window_);
  DCHECK(!window_->IsRootWindow());
  if (increment_requests)
    window_->OnScopedWindowCaptureRequestAdded();
  window_->AddObserver(this);
}

Window* ScopedWindowCaptureRequest::DetachFromCurrentWindow(
    bool decrement_requests) {
  Window* result = window_;
  if (window_) {
    window_->RemoveObserver(this);
    if (decrement_requests)
      window_->OnScopedWindowCaptureRequestRemoved();
    window_ = nullptr;
  }
  return result;
}

}  // namespace aura
