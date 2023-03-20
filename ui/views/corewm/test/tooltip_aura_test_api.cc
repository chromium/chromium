// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/test/tooltip_aura_test_api.h"

#include "ui/accessibility/ax_node_data.h"
#include "ui/base/owned_window_anchor.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/render_text.h"
#include "ui/views/corewm/tooltip_aura.h"

namespace views::corewm::test {

const gfx::RenderText* TooltipAuraTestApi::GetRenderText() const {
  return tooltip_aura_->GetRenderTextForTest();
}

void TooltipAuraTestApi::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  return tooltip_aura_->GetAccessibleNodeDataForTest(node_data);
}

gfx::Rect TooltipAuraTestApi::GetTooltipBounds(const gfx::Size& tooltip_size,
                                               const gfx::Point& anchor_point,
                                               const TooltipTrigger trigger) {
  ui::OwnedWindowAnchor anchor;
  return tooltip_aura_->GetTooltipBounds(tooltip_size, anchor_point, trigger,
                                         &anchor);
}

}  // namespace views::corewm::test
