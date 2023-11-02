// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_text_attributes.h"

namespace ui {

AXTextAttributes::AXTextAttributes() = default;

AXTextAttributes::~AXTextAttributes() = default;

AXTextAttributes::AXTextAttributes(AXTextAttributes&& other)
    : background_color(other.background_color),
      color(other.color),
      invalid_state(other.invalid_state),
      overline_style(other.overline_style),
      strikethrough_style(other.strikethrough_style),
      text_direction(other.text_direction),
      text_position(other.text_position),
      text_style(other.text_style),
      underline_style(other.underline_style),
      font_size(other.font_size),
      font_weight(other.font_weight),
      font_family(std::move(other.font_family)),
      marker_types(std::move(other.marker_types)),
      highlight_types(std::move(other.highlight_types)) {}

AXTextAttributes& AXTextAttributes::operator=(AXTextAttributes&& other) {
  if (this == &other)
    return *this;

  background_color = other.background_color;
  color = other.color;
  invalid_state = other.invalid_state;
  overline_style = other.overline_style;
  strikethrough_style = other.strikethrough_style;
  text_direction = other.text_direction;
  text_position = other.text_position;
  text_style = other.text_style;
  underline_style = other.underline_style;
  font_size = other.font_size;
  font_weight = other.font_weight;
  font_family = other.font_family;
  marker_types = other.marker_types;
  highlight_types = other.highlight_types;

  return *this;
}

bool AXTextAttributes::operator==(const AXTextAttributes& other) const {
  return background_color == other.background_color && color == other.color &&
         invalid_state == other.invalid_state &&
         overline_style == other.overline_style &&
         strikethrough_style == other.strikethrough_style &&
         text_direction == other.text_direction &&
         text_position == other.text_position && font_size == other.font_size &&
         font_weight == other.font_weight && text_style == other.text_style &&
         underline_style == other.underline_style &&
         font_family == other.font_family &&
         marker_types == other.marker_types &&
         highlight_types == other.highlight_types;
}

bool AXTextAttributes::operator!=(const AXTextAttributes& other) const {
  return !operator==(other);
}

bool AXTextAttributes::IsUnset() const {
  return background_color == kUnsetValue && invalid_state == kUnsetValue &&
         overline_style == kUnsetValue && strikethrough_style == kUnsetValue &&
         text_position == kUnsetValue && font_size == kUnsetValue &&
         font_weight == kUnsetValue && text_style == kUnsetValue &&
         underline_style == kUnsetValue && font_family.length() == 0 &&
         marker_types.size() == 0 && highlight_types.size() == 0;
}

}  // namespace ui
