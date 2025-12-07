// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_SPAN_STYLE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_SPAN_STYLE_H_

#include <optional>

#include "ui/base/ime/ime_text_span.h"

namespace ui {

struct SpanStyle {
  struct Style {
    bool operator==(const Style& other) const = default;

    ImeTextSpan::Type type;
    ImeTextSpan::Thickness thickness;
  };

  bool operator==(const SpanStyle& other) const = default;

  // Byte offset.
  uint32_t index;
  // Length in bytes.
  uint32_t length;
  // One of preedit_style.
  std::optional<Style> style;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_SPAN_STYLE_H_
