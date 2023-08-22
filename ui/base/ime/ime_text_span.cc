// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/ime_text_span.h"

#include <string>
#include <vector>

namespace ui {

ImeTextSpan::ImeTextSpan(Type type,
                         size_t start_offset,
                         size_t end_offset,
                         Thickness thickness,
                         UnderlineStyle underline_style,
                         SkColor background_color,
                         SkColor suggestion_highlight_color,
                         const std::vector<std::string>& suggestions,
                         SkColor text_color)
    : type(type),
      start_offset(start_offset),
      end_offset(end_offset),
      thickness(thickness),
      underline_style(underline_style),
      text_color(text_color),
      background_color(background_color),
      suggestion_highlight_color(suggestion_highlight_color),
      suggestions(suggestions) {}

ImeTextSpan::ImeTextSpan(const ImeTextSpan& rhs) = default;

ImeTextSpan::~ImeTextSpan() = default;

}  // namespace ui
