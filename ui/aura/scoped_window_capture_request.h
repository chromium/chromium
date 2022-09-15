// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SCOPED_WINDOW_CAPTURE_REQUEST_H_
#define UI_AURA_SCOPED_WINDOW_CAPTURE_REQUEST_H_

#include "base/memory/raw_ptr.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/window_observer.h"

namespace aura {

class Window;

// A scoped move-only object which is associated with a request to make a
// non-root window individually capturable by a FrameSinkVideoCapturer. This
// request is tracked as long as this object lives. Once all requests are
// destroyed, the window will no longer be uniquely identifiable by a
// viz::SubtreeCaptureId, and can no longer be individually capturable by the
// FrameSinkVideoCapturer.
// Note that making a window capturable forces the layer tree root at its layer
// to be promoted to a render surface that draw into a render pass.
// See https://crbug.com/1143930 for more details.
class AURA_EXPORT ScopedWindowCaptureRequest : public WindowObserver {
 public:
  // Creates an empty request that doesn't affect any window.
  ScopedWindowCaptureRequest() = default;
  ScopedWindowCaptureRequest(ScopedWindowCaptureRequest&& other);
  ScopedWindowCaptureRequest& operator=(ScopedWindowCaptureRequest&& rhs);
  ~ScopedWindowCaptureRequest() override;

  Window* window() const { return window_; }

  viz::SubtreeCaptureId GetCaptureId() const;

  // WindowObserver:
  void OnWindowDestroying(Window* window) override;

 private:
  friend class Window;

  // Private so it can only be called through Window::MakeWindowCapturable().
  explicit ScopedWindowCaptureRequest(Window* window);

  // Attaches to the current |window_| by observing it. If |increment_requests|
  // is true (meaning this is a new request, rather than an existing one being
  // moved into |this|), OnScopedWindowCaptureRequestAdded() will be called on
  // |window_|. |window_| must be valid.
  void AttachToCurrentWindow(bool increment_requests);

  // Detaches from the current |window_| (if any) by ending observing it. If
  // |decrement_requests| is true (meaning this request is actually ending
  // rather than being moved from |this|), OnScopedWindowCaptureRequestRemoved()
  // will be called on |window_|.
  // |window_| will be reset to |nullptr|, and its old value will be returned.
  Window* DetachFromCurrentWindow(bool decrement_requests);

  // The window on which this request has been made. Can be |nullptr| if this is
  // an empty request (created by the default ctor), or if this object was
  // std::move()'d from.
  raw_ptr<Window> window_ = nullptr;
};

}  // namespace aura

#endif  // UI_AURA_SCOPED_WINDOW_CAPTURE_REQUEST_H_
