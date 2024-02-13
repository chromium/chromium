// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SCROLLBAR_SCROLL_BAR_VIEWS_H_
#define UI_VIEWS_CONTROLS_SCROLLBAR_SCROLL_BAR_VIEWS_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"
#include "ui/views/view.h"

namespace gfx {
class Canvas;
}

namespace views {

// Views implementation for the scrollbar.
class VIEWS_EXPORT ScrollBarViews : public ScrollBar {
  METADATA_HEADER(ScrollBarViews, ScrollBar)

 public:
  // Creates new scrollbar, either horizontal or vertical.
  explicit ScrollBarViews(Orientation orientation = Orientation::kHorizontal);

  ScrollBarViews(const ScrollBarViews&) = delete;
  ScrollBarViews& operator=(const ScrollBarViews&) = delete;

  ~ScrollBarViews() override;

  static int GetVerticalScrollBarWidth(const ui::NativeTheme* theme);

 protected:
  // View overrides:
  void OnPaint(gfx::Canvas* canvas) override;

  // ScrollBar overrides:
  int GetThickness() const override;

  // Returns the area for the track. This is the area of the scrollbar minus
  // the size of the arrow buttons.
  gfx::Rect GetTrackBounds() const override;

 private:
  // The scroll bar buttons (Up/Down, Left/Right).
  raw_ptr<Button> prev_button_;
  raw_ptr<Button> next_button_;

  ui::NativeTheme::Part part_;
  ui::NativeTheme::State state_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_SCROLLBAR_SCROLL_BAR_VIEWS_H_
