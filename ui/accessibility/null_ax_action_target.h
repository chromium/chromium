// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_NULL_AX_ACTION_TARGET_H_
#define UI_ACCESSIBILITY_NULL_AX_ACTION_TARGET_H_

#include <optional>

#include "ui/accessibility/ax_action_target.h"
#include "ui/accessibility/ax_export.h"

namespace ui {

// A do-nothing action target.
class AX_EXPORT NullAXActionTarget : public AXActionTarget {
 public:
  NullAXActionTarget() = default;
  ~NullAXActionTarget() override = default;

 protected:
  // AXActionTarget overrides.
  Type GetType() const override;
  bool PerformAction(const AXActionData& action_data) const override;
  gfx::Rect GetRelativeBounds() const override;
  gfx::Point GetScrollOffset() const override;
  gfx::Point MinimumScrollOffset() const override;
  gfx::Point MaximumScrollOffset() const override;
  void SetScrollOffset(const gfx::Point& point) const override;
  bool SetSelection(
      const AXActionTarget* anchor_object,
      int anchor_offset,
      std::optional<ax::mojom::OffsetType> anchor_offset_type,
      const AXActionTarget* focus_object,
      int focus_offset,
      std::optional<ax::mojom::OffsetType> focus_offset_type) const override;
  bool ScrollToMakeVisible(
      ax::mojom::ScrollAlignment horizontal_scroll_alignment,
      ax::mojom::ScrollAlignment vertical_scroll_alignment,
      ax::mojom::ScrollBehavior scroll_behavior) const override;
  bool ScrollToMakeVisibleWithSubFocus(
      const gfx::Rect& rect,
      ax::mojom::ScrollAlignment horizontal_scroll_alignment,
      ax::mojom::ScrollAlignment vertical_scroll_alignment,
      ax::mojom::ScrollBehavior scroll_behavior) const override;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_NULL_AX_ACTION_TARGET_H_
