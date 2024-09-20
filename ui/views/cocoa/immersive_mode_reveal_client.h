// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_IMMERSIVE_MODE_REVEAL_CLIENT_H_
#define UI_VIEWS_COCOA_IMMERSIVE_MODE_REVEAL_CLIENT_H_

#include "ui/views/views_export.h"

namespace views {

class VIEWS_EXPORT ImmersiveModeRevealClient {
 public:
  ImmersiveModeRevealClient() = default;
  virtual ~ImmersiveModeRevealClient() = default;

  virtual void OnImmersiveModeToolbarRevealChanged(bool is_revealed) = 0;
  virtual void OnImmersiveModeMenuBarRevealChanged(double reveal_amount) = 0;
  virtual void OnAutohidingMenuBarHeightChanged(int menu_bar_height) = 0;
};

}  // namespace views

#endif  // UI_VIEWS_COCOA_IMMERSIVE_MODE_REVEAL_CLIENT_H_
