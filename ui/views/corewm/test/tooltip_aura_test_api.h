// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_TEST_TOOLTIP_AURA_TEST_API_H_
#define UI_VIEWS_COREWM_TEST_TOOLTIP_AURA_TEST_API_H_

#include <stddef.h>

#include "base/memory/raw_ptr.h"

namespace gfx {
class Rect;
class RenderText;
class Size;
}

namespace ui {
struct AXNodeData;
}

namespace views {
namespace corewm {
class TooltipAura;
struct TooltipPosition;

namespace test {

class TooltipAuraTestApi {
 public:
  explicit TooltipAuraTestApi(TooltipAura* tooltip_aura)
      : tooltip_aura_(tooltip_aura) {}

  TooltipAuraTestApi(const TooltipAuraTestApi&) = delete;
  TooltipAuraTestApi& operator=(const TooltipAuraTestApi&) = delete;

  gfx::RenderText* GetRenderText();

  void GetAccessibleNodeData(ui::AXNodeData* node_data);

  gfx::Rect GetTooltipBounds(const gfx::Size& tooltip_size,
                             const TooltipPosition& position);

 private:
  raw_ptr<TooltipAura> tooltip_aura_;
};

}  // namespace test
}  // namespace corewm
}  // namespace views

#endif  // UI_VIEWS_COREWM_TEST_TOOLTIP_AURA_TEST_API_H_
