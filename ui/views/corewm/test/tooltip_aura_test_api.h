// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_TEST_TOOLTIP_AURA_TEST_API_H_
#define UI_VIEWS_COREWM_TEST_TOOLTIP_AURA_TEST_API_H_

#include <stddef.h>

#include "base/macros.h"

namespace gfx {
class RenderText;
}

namespace ui {
struct AXNodeData;
}

namespace views {
namespace corewm {
class TooltipAura;

namespace test {

class TooltipAuraTestApi {
 public:
  explicit TooltipAuraTestApi(TooltipAura* tooltip_aura)
      : tooltip_aura_(tooltip_aura) {}

  gfx::RenderText* GetRenderText();

  void GetAccessibleNodeData(ui::AXNodeData* node_data);

 private:
  TooltipAura* tooltip_aura_;

  DISALLOW_COPY_AND_ASSIGN(TooltipAuraTestApi);
};

}  // namespace test
}  // namespace corewm
}  // namespace views

#endif  // UI_VIEWS_COREWM_TEST_TOOLTIP_AURA_TEST_API_H_
