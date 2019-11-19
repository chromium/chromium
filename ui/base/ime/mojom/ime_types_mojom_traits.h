// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_MOJOM_IME_TYPES_MOJOM_TRAITS_H_
#define UI_BASE_IME_MOJOM_IME_TYPES_MOJOM_TRAITS_H_

#include <vector>

#include "ui/base/ime/ime_text_span.h"
#include "ui/base/ime/mojom/ime_types.mojom.h"
#include "ui/base/ime/text_input_type.h"

namespace mojo {

template <>
struct EnumTraits<ui::mojom::TextInputType, ui::TextInputType> {
  static ui::mojom::TextInputType ToMojom(ui::TextInputType text_input_type);
  static bool FromMojom(ui::mojom::TextInputType input, ui::TextInputType* out);
};

template <>
struct StructTraits<ui::mojom::ImeTextSpanDataView, ui::ImeTextSpan> {
  static ui::ImeTextSpan::Type type(const ui::ImeTextSpan& c) { return c.type; }
  static uint32_t start_offset(const ui::ImeTextSpan& c) {
    return c.start_offset;
  }
  static uint32_t end_offset(const ui::ImeTextSpan& c) { return c.end_offset; }
  static uint32_t underline_color(const ui::ImeTextSpan& c) {
    return c.underline_color;
  }
  static ui::ImeTextSpan::Thickness thickness(const ui::ImeTextSpan& i) {
    return i.thickness;
  }
  static uint32_t background_color(const ui::ImeTextSpan& c) {
    return c.background_color;
  }
  static uint32_t suggestion_highlight_color(const ui::ImeTextSpan& c) {
    return c.suggestion_highlight_color;
  }
  static bool remove_on_finish_composing(const ui::ImeTextSpan& c) {
    return c.remove_on_finish_composing;
  }
  static std::vector<std::string> suggestions(const ui::ImeTextSpan& c) {
    return c.suggestions;
  }
  static bool Read(ui::mojom::ImeTextSpanDataView data, ui::ImeTextSpan* out);
};

template <>
struct EnumTraits<ui::mojom::ImeTextSpanType, ui::ImeTextSpan::Type> {
  static ui::mojom::ImeTextSpanType ToMojom(
      ui::ImeTextSpan::Type ime_text_span_type);
  static bool FromMojom(ui::mojom::ImeTextSpanType input,
                        ui::ImeTextSpan::Type* out);
};

template <>
struct EnumTraits<ui::mojom::ImeTextSpanThickness, ui::ImeTextSpan::Thickness> {
  static ui::mojom::ImeTextSpanThickness ToMojom(
      ui::ImeTextSpan::Thickness thickness);
  static bool FromMojom(ui::mojom::ImeTextSpanThickness input,
                        ui::ImeTextSpan::Thickness* out);
};

}  // namespace mojo

#endif  // UI_BASE_IME_MOJOM_IME_TYPES_MOJOM_TRAITS_H_
