// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_NATIVE_FRAME_VIEW_H_
#define UI_VIEWS_WINDOW_NATIVE_FRAME_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/window/non_client_view.h"

namespace views {

class Widget;

class VIEWS_EXPORT NativeFrameView : public NonClientFrameView {
  METADATA_HEADER(NativeFrameView, NonClientFrameView)

 public:
  explicit NativeFrameView(Widget* frame);
  NativeFrameView(const NativeFrameView&) = delete;
  NativeFrameView& operator=(const NativeFrameView&) = delete;
  ~NativeFrameView() override;

  // NonClientFrameView overrides:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override;
  void ResetWindowControls() override;
  void UpdateWindowIcon() override;
  void UpdateWindowTitle() override;
  void SizeConstraintsChanged() override;

  // View overrides:
  gfx::Size CalculatePreferredSize(
      const SizeBounds& available_size) const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;

 private:
  // Our containing frame.
  raw_ptr<Widget> frame_;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, NativeFrameView, NonClientFrameView)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, NativeFrameView)

#endif  // UI_VIEWS_WINDOW_NATIVE_FRAME_VIEW_H_
