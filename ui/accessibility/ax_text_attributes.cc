// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_text_attributes.h"

#include "ui/accessibility/ax_node_data.h"

namespace ui {

namespace {

void UpdateProperty(ax::mojom::IntAttribute attribute,
                    const AXNodeData& node_data,
                    int* value) {
  int maybe_value = node_data.GetIntAttribute(attribute);
  if (maybe_value || node_data.HasIntAttribute(attribute)) {
    *value = maybe_value;
  }
}

void UpdateProperty(ax::mojom::FloatAttribute attribute,
                    const AXNodeData& node_data,
                    float* value) {
  int maybe_value = node_data.GetFloatAttribute(attribute);
  if (maybe_value || node_data.HasFloatAttribute(attribute)) {
    *value = maybe_value;
  }
}

}  // namespace

AXTextAttributes::AXTextAttributes() = default;

AXTextAttributes::~AXTextAttributes() = default;

AXTextAttributes::AXTextAttributes(const AXNodeData& node_data) {
  UpdateProperty(ax::mojom::IntAttribute::kBackgroundColor, node_data,
                 &background_color);
  UpdateProperty(ax::mojom::IntAttribute::kColor, node_data, &color);
  UpdateProperty(ax::mojom::IntAttribute::kInvalidState, node_data,
                 &invalid_state);
  UpdateProperty(ax::mojom::IntAttribute::kTextOverlineStyle, node_data,
                 &overline_style);
  UpdateProperty(ax::mojom::IntAttribute::kTextDirection, node_data,
                 &text_direction);
  UpdateProperty(ax::mojom::IntAttribute::kTextPosition, node_data,
                 &text_position);
  UpdateProperty(ax::mojom::IntAttribute::kTextStrikethroughStyle, node_data,
                 &strikethrough_style);
  UpdateProperty(ax::mojom::IntAttribute::kTextStyle, node_data, &text_style);
  UpdateProperty(ax::mojom::IntAttribute::kTextUnderlineStyle, node_data,
                 &underline_style);
  UpdateProperty(ax::mojom::FloatAttribute::kFontSize, node_data, &font_size);
  UpdateProperty(ax::mojom::FloatAttribute::kFontWeight, node_data,
                 &font_weight);
  font_family =
      node_data.GetStringAttribute(ax::mojom::StringAttribute::kFontFamily);
  marker_types =
      node_data.GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerTypes);
  highlight_types = node_data.GetIntListAttribute(
      ax::mojom::IntListAttribute::kHighlightTypes);
}

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

bool AXTextAttributes::HasTextStyle(
    ax::mojom::TextStyle text_style_enum) const {
  return text_style != kUnsetValue &&
         (static_cast<uint32_t>(text_style) &
          (1U << static_cast<uint32_t>(text_style_enum))) != 0;
}

}  // namespace ui
