// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TEXT_ATTRIBUTES_H_
#define UI_ACCESSIBILITY_AX_TEXT_ATTRIBUTES_H_

#include <string>

#include "ui/accessibility/ax_base_export.h"

namespace ui {

// A compact representation of text styles on an AXNode. This data represents
// a snapshot at a given time and is not intended to be held for periods of
// time. For this reason, it is a move-only class, to encourage deliberate
// short-term usage.
struct AX_BASE_EXPORT AXTextAttributes {
  AXTextAttributes();

  // Move-only class, explicitly delete copy-construction and assignment
  AXTextAttributes(const AXTextAttributes& other) = delete;
  AXTextAttributes& operator=(const AXTextAttributes&) = delete;

  // Move constructor and assignment
  AXTextAttributes(AXTextAttributes&& other);
  AXTextAttributes& operator=(AXTextAttributes&& other);

  bool operator==(const AXTextAttributes& other) const;

  bool operator!=(const AXTextAttributes& other) const;

  bool IsUnset() const;

  int32_t background_color;
  int32_t color;
  int32_t invalid_state;
  int32_t overline_style;
  int32_t strikethrough_style;
  int32_t text_direction;
  int32_t text_position;
  int32_t text_style;
  int32_t underline_style;
  float font_size;
  float font_weight;
  std::string font_family;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TEXT_ATTRIBUTES_H_
