// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_MENU_SEPARATOR_TYPES_H_
#define UI_BASE_MODELS_MENU_SEPARATOR_TYPES_H_

namespace ui {

// For a separator we have the following types.
enum MenuSeparatorType {
  // Normal - top to bottom: Spacing, line, spacing.
  NORMAL_SEPARATOR,

  // Double thickness - top to bottom: Spacing, line, spacing.
  DOUBLE_SEPARATOR,

  // Upper - top to bottom: Line, spacing.
  UPPER_SEPARATOR,

  // Lower - top to bottom: Spacing, line.
  LOWER_SEPARATOR,

  // Spacing - top to bottom: Spacing only.
  SPACING_SEPARATOR,

  // Vertical separator within a row.
  VERTICAL_SEPARATOR,

  // Separator with left padding - top to bottom: Line only,
  //                               horizontal: Starts after left padding.
  PADDED_SEPARATOR,
};

}  // namespace ui

#endif  // UI_BASE_MODELS_MENU_SEPARATOR_TYPES_H_
