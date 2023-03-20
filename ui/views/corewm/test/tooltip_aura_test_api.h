// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_TEST_TOOLTIP_AURA_TEST_API_H_
#define UI_VIEWS_COREWM_TEST_TOOLTIP_AURA_TEST_API_H_

#include <stddef.h>

#include "base/memory/raw_ptr.h"

namespace gfx {
class Point;
class Rect;
class RenderText;
class Size;
}

namespace ui {
struct AXNodeData;
}

namespace views::corewm {
class TooltipAura;
enum class TooltipTrigger;

namespace test {

class TooltipAuraTestApi {
 public:
  explicit TooltipAuraTestApi(TooltipAura* tooltip_aura)
      : tooltip_aura_(tooltip_aura) {}

  TooltipAuraTestApi(const TooltipAuraTestApi&) = delete;
  TooltipAuraTestApi& operator=(const TooltipAuraTestApi&) = delete;

  const gfx::RenderText* GetRenderText() const;

  void GetAccessibleNodeData(ui::AXNodeData* node_data);

  gfx::Rect GetTooltipBounds(const gfx::Size& tooltip_size,
                             const gfx::Point& anchor_point,
                             const TooltipTrigger trigger);

 private:
  raw_ptr<TooltipAura> tooltip_aura_;
};

}  // namespace test
}  // namespace views::corewm

#endif  // UI_VIEWS_COREWM_TEST_TOOLTIP_AURA_TEST_API_H_
