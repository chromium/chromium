// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/native_frame_view.h"

#include "build/build_config.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "ui/views/win/hwnd_util.h"
#endif

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeFrameView, public:

// static
const char NativeFrameView::kViewClassName[] = "NativeFrameView";

NativeFrameView::NativeFrameView(Widget* frame) : frame_(frame) {}

NativeFrameView::~NativeFrameView() = default;

////////////////////////////////////////////////////////////////////////////////
// NativeFrameView, NonClientFrameView overrides:

gfx::Rect NativeFrameView::GetBoundsForClientView() const {
  return gfx::Rect(0, 0, width(), height());
}

gfx::Rect NativeFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
#if defined(OS_WIN)
  return views::GetWindowBoundsForClientBounds(
      static_cast<View*>(const_cast<NativeFrameView*>(this)), client_bounds);
#else
  // Enforce minimum size (1, 1) in case that |client_bounds| is passed with
  // empty size.
  gfx::Rect window_bounds = client_bounds;
  if (window_bounds.IsEmpty())
    window_bounds.set_size(gfx::Size(1,1));
  return window_bounds;
#endif
}

int NativeFrameView::NonClientHitTest(const gfx::Point& point) {
  return frame_->client_view()->NonClientHitTest(point);
}

void NativeFrameView::GetWindowMask(const gfx::Size& size,
                                    SkPath* window_mask) {
  // Nothing to do, we use the default window mask.
}

void NativeFrameView::ResetWindowControls() {
  // Nothing to do.
}

void NativeFrameView::UpdateWindowIcon() {
  // Nothing to do.
}

void NativeFrameView::UpdateWindowTitle() {
  // Nothing to do.
}

void NativeFrameView::SizeConstraintsChanged() {
  // Nothing to do.
}

gfx::Size NativeFrameView::CalculatePreferredSize() const {
  gfx::Size client_preferred_size = frame_->client_view()->GetPreferredSize();
#if defined(OS_WIN)
  // Returns the client size. On Windows, this is the expected behavior for
  // native frames (see |NativeWidgetWin::WidgetSizeIsClientSize()|), while
  // other platforms currently always return client bounds from
  // |GetWindowBoundsForClientBounds()|.
  return client_preferred_size;
#else
  return frame_->non_client_view()->GetWindowBoundsForClientBounds(
      gfx::Rect(client_preferred_size)).size();
#endif
}

gfx::Size NativeFrameView::GetMinimumSize() const {
  return frame_->client_view()->GetMinimumSize();
}

gfx::Size NativeFrameView::GetMaximumSize() const {
  return frame_->client_view()->GetMaximumSize();
}

const char* NativeFrameView::GetClassName() const {
  return kViewClassName;
}

}  // namespace views
