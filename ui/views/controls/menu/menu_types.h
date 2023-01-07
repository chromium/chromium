// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_TYPES_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_TYPES_H_

namespace views {

// Where a popup menu should be anchored to for non-RTL languages. The opposite
// position will be used if base::i18n::IsRTL() is true. The Bubble flags are
// used when the menu should get enclosed by a bubble.
enum class MenuAnchorPosition {
  kTopLeft,
  kTopRight,
  kBottomCenter,
  kBubbleTopLeft,
  kBubbleTopRight,
  kBubbleLeft,
  kBubbleRight,
  kBubbleBottomLeft,
  kBubbleBottomRight,
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_TYPES_H_
