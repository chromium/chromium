// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TEXT_ATTRIBUTES_H_
#define UI_ACCESSIBILITY_AX_TEXT_ATTRIBUTES_H_

#include <string>
#include <vector>

#include "ui/accessibility/ax_base_export.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"

namespace ui {

struct AXNodeData;

// A compact representation of text attributes, such as spelling markers and
// style information, on an `AXNode`. This data represents a snapshot at a given
// time and is not intended to be held for periods of time. For this reason, it
// is a move-only class, to encourage deliberate short-term usage.
struct AX_BASE_EXPORT AXTextAttributes final {
  // For numeric attributes, the value to be used when a particular attribute is
  // not set on the `AXNode`, or its value is otherwise unknown.
  static constexpr int kUnsetValue = -1;

  AXTextAttributes();
  ~AXTextAttributes();

  explicit AXTextAttributes(const AXNodeData& node_data);

  AXTextAttributes(const AXTextAttributes& other) = delete;
  AXTextAttributes& operator=(const AXTextAttributes&) = delete;

  AXTextAttributes(AXTextAttributes&& other);
  AXTextAttributes& operator=(AXTextAttributes&& other);

  bool operator==(const AXTextAttributes& other) const;

  bool operator!=(const AXTextAttributes& other) const;

  bool IsUnset() const;

  bool HasTextStyle(const ax::mojom::TextStyle text_style_enum) const;

  int32_t background_color = kUnsetValue;
  int32_t color = kUnsetValue;
  int32_t invalid_state = kUnsetValue;
  int32_t overline_style = kUnsetValue;
  int32_t strikethrough_style = kUnsetValue;
  int32_t text_direction = kUnsetValue;
  int32_t text_position = kUnsetValue;
  int32_t text_style = kUnsetValue;
  int32_t underline_style = kUnsetValue;
  float font_size = kUnsetValue;
  float font_weight = kUnsetValue;
  std::string font_family;
  std::vector<int32_t> marker_types;
  std::vector<int32_t> highlight_types;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TEXT_ATTRIBUTES_H_
