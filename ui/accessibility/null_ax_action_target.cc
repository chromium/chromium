// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/null_ax_action_target.h"

namespace ui {

AXActionTarget::Type NullAXActionTarget::GetType() const {
  return AXActionTarget::Type::kNull;
}

bool NullAXActionTarget::PerformAction(const AXActionData& action_data) const {
  return false;
}

gfx::Rect NullAXActionTarget::GetRelativeBounds() const {
  return gfx::Rect();
}

gfx::Point NullAXActionTarget::GetScrollOffset() const {
  return gfx::Point();
}

gfx::Point NullAXActionTarget::MinimumScrollOffset() const {
  return gfx::Point();
}

gfx::Point NullAXActionTarget::MaximumScrollOffset() const {
  return gfx::Point();
}

void NullAXActionTarget::SetScrollOffset(const gfx::Point& point) const {}

bool NullAXActionTarget::SetSelection(const AXActionTarget* anchor_object,
                                      int anchor_offset,
                                      const AXActionTarget* focus_object,
                                      int focus_offset) const {
  return false;
}

bool NullAXActionTarget::ScrollToMakeVisible() const {
  return false;
}

bool NullAXActionTarget::ScrollToMakeVisibleWithSubFocus(
    const gfx::Rect& rect,
    ax::mojom::ScrollAlignment horizontal_scroll_alignment,
    ax::mojom::ScrollAlignment vertical_scroll_alignment,
    ax::mojom::ScrollBehavior scroll_behavior) const {
  return false;
}

}  // namespace ui
