// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_COLOR_ID_H_
#define UI_NATIVE_THEME_NATIVE_THEME_COLOR_ID_H_

// Clang format mangles lists like the below badly.
// clang-format off
#define NATIVE_THEME_CROSS_PLATFORM_COLOR_IDS                                  \
  OP(kColorId_DefaultIconColor),                                               \
  OP(kColorId_FocusedBorderColor),                                             \
  OP(kColorId_FocusedMenuItemBackgroundColor),                                 \
  OP(kColorId_MenuBackgroundColor),                                            \
  OP(kColorId_MenuIconColor),                                                  \
  OP(kColorId_MenuSeparatorColor),                                             \
  OP(kColorId_OverlayScrollbarThumbFill),                                      \
  OP(kColorId_OverlayScrollbarThumbHoveredFill),                               \
  OP(kColorId_OverlayScrollbarThumbHoveredStroke),                             \
  OP(kColorId_OverlayScrollbarThumbStroke),                                    \
  OP(kColorId_ProminentButtonColor),                                           \
  OP(kColorId_TextOnProminentButtonColor),                                     \
  OP(kColorId_ThrobberSpinningColor),                                          \
  OP(kColorId_ThrobberWaitingColor),                                           \
  OP(kColorId_WindowBackground)

#define NATIVE_THEME_COLOR_IDS NATIVE_THEME_CROSS_PLATFORM_COLOR_IDS

// clang-format on

#endif  // UI_NATIVE_THEME_NATIVE_THEME_COLOR_ID_H_
