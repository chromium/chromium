// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_WINDOW_FRAME_PROVIDER_GTK_H_
#define UI_GTK_WINDOW_FRAME_PROVIDER_GTK_H_

#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/linux_ui/window_frame_provider.h"

namespace gtk {

class WindowFrameProviderGtk : public views::WindowFrameProvider {
 public:
  explicit WindowFrameProviderGtk(bool solid_frame);

  WindowFrameProviderGtk(const WindowFrameProviderGtk&) = delete;
  WindowFrameProviderGtk& operator=(const WindowFrameProviderGtk&) = delete;

  ~WindowFrameProviderGtk() override;

  // views::WindowFrameProvider:
  int GetTopCornerRadius() override;
  gfx::Insets GetFrameThickness() override;
  void PaintWindowFrame(gfx::Canvas* canvas,
                        const gfx::Rect& rect,
                        int top_area_height,
                        bool focused) override;

 private:
  // Paint the window frame and update any metrics (like the frame thickness)
  // based on it.  This is a no-op if the input parameters haven't changed.
  void MaybeUpdateBitmaps();

  int BitmapSizePx() const;

  // Input parameters used for drawing.
  const bool solid_frame_;
  float scale_ = 0;
  std::string theme_name_;

  // These are texture maps that we will sample from to draw the frame.  The
  // corners are drawn directly and the edges are tiled.
  SkBitmap focused_bitmap_;
  SkBitmap unfocused_bitmap_;

  // Metrics calculated based on the bitmaps.
  int top_corner_radius_ = 0;
  int frame_size_px_ = 0;
  gfx::Insets frame_thickness_dip_;
  gfx::Insets frame_thickness_px_;
};

}  // namespace gtk

#endif  // UI_GTK_WINDOW_FRAME_PROVIDER_GTK_H_
