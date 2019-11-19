// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_TYPES_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_TYPES_H_

namespace views {

// Where a popup menu should be anchored to for non-RTL languages. The opposite
// position will be used if base::i18n:IsRTL() is true. The BUBBLE flags are
// used when the menu should get enclosed by a bubble. The Fixed flags are
// used for the menus that have a fixed anchor position.
enum class MenuAnchorPosition {
  kTopLeft,
  kTopRight,
  kBottomCenter,
  kBubbleAbove,
  kBubbleLeft,
  kBubbleRight,
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_TYPES_H_
