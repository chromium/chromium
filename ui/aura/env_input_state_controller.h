// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_ENV_INPUT_STATE_CONTROLLER_H_
#define UI_AURA_ENV_INPUT_STATE_CONTROLLER_H_

#include <stdint.h>

#include "base/macros.h"
#include "ui/aura/aura_export.h"

namespace gfx {
class Point;
}

namespace ui {
class MouseEvent;
class TouchEvent;
}

namespace aura {
namespace test {
class EnvTestHelper;
}

class Env;
class Window;

class AURA_EXPORT EnvInputStateController {
 public:
  explicit EnvInputStateController(Env* env);
  ~EnvInputStateController();

  void UpdateStateForMouseEvent(const Window* window,
                                const ui::MouseEvent& event);
  void UpdateStateForTouchEvent(const ui::TouchEvent& event);
  void SetLastMouseLocation(const Window* root_window,
                            const gfx::Point& location_in_root) const;

 private:
  friend class test::EnvTestHelper;

  Env* env_;
  // Touch ids that are currently down.
  uint32_t touch_ids_down_ = 0;

  DISALLOW_COPY_AND_ASSIGN(EnvInputStateController);
};

}  // namespace aura

#endif  // UI_AURA_ENV_INPUT_STATE_CONTROLLER_H_
