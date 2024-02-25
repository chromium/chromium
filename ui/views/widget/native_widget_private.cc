// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/native_widget_private.h"

#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace views {

const char kWidgetIdentifierKey[] = "kWidgetIdentifierKey";

namespace internal {

// static
gfx::Rect NativeWidgetPrivate::ConstrainBoundsToDisplayWorkArea(
    const gfx::Rect& bounds) {
  gfx::Rect new_bounds(bounds);
  gfx::Rect work_area =
      display::Screen::GetScreen()->GetDisplayMatching(bounds).work_area();
  if (!work_area.IsEmpty())
    new_bounds.AdjustToFit(work_area);
  return new_bounds;
}

void NativeWidgetPrivate::PaintAsActiveChanged() {}

void NativeWidgetPrivate::ShowEmojiPanel() {
  ui::ShowEmojiPanel();
}

bool NativeWidgetPrivate::IsMoveLoopSupported() const {
  return true;
}

}  // namespace internal

}  // namespace views
