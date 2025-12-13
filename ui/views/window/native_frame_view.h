// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_NATIVE_FRAME_VIEW_H_
#define UI_VIEWS_WINDOW_NATIVE_FRAME_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/window/frame_view.h"

namespace views {

class Widget;

class VIEWS_EXPORT NativeFrameView : public FrameView {
  METADATA_HEADER(NativeFrameView, FrameView)

 public:
  explicit NativeFrameView(Widget* widget);
  NativeFrameView(const NativeFrameView&) = delete;
  NativeFrameView& operator=(const NativeFrameView&) = delete;
  ~NativeFrameView() override;

  // FrameView overrides:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;

  // View overrides:
  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;

 private:
  // Our containing frame.
  raw_ptr<Widget> widget_;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, NativeFrameView, FrameView)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, NativeFrameView)

#endif  // UI_VIEWS_WINDOW_NATIVE_FRAME_VIEW_H_
