// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_MOJOM_IME_TYPES_MOJOM_TRAITS_H_
#define UI_BASE_IME_MOJOM_IME_TYPES_MOJOM_TRAITS_H_

#include <vector>

#include "base/component_export.h"
#include "ui/base/ime/ime_text_span.h"
#include "ui/base/ime/mojom/ime_types.mojom-shared.h"
#include "ui/base/ime/text_input_action.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(IME_SHARED_MOJOM_TRAITS)
    EnumTraits<ui::mojom::TextInputMode, ui::TextInputMode> {
  static ui::mojom::TextInputMode ToMojom(ui::TextInputMode text_input_mode);
  static bool FromMojom(ui::mojom::TextInputMode input, ui::TextInputMode* out);
};

template <>
struct COMPONENT_EXPORT(IME_SHARED_MOJOM_TRAITS)
    EnumTraits<ui::mojom::TextInputAction, ui::TextInputAction> {
  static ui::mojom::TextInputAction ToMojom(
      ui::TextInputAction text_input_action);
  static bool FromMojom(ui::mojom::TextInputAction input,
                        ui::TextInputAction* out);
};

template <>
struct COMPONENT_EXPORT(IME_SHARED_MOJOM_TRAITS)
    EnumTraits<ui::mojom::TextInputType, ui::TextInputType> {
  static ui::mojom::TextInputType ToMojom(ui::TextInputType text_input_type);
  static bool FromMojom(ui::mojom::TextInputType input, ui::TextInputType* out);
};

template <>
struct COMPONENT_EXPORT(IME_SHARED_MOJOM_TRAITS)
    StructTraits<ui::mojom::ImeTextSpanDataView, ui::ImeTextSpan> {
  static ui::ImeTextSpan::Type type(const ui::ImeTextSpan& c) { return c.type; }
  static size_t start_offset(const ui::ImeTextSpan& c) {
    return c.start_offset;
  }
  static size_t end_offset(const ui::ImeTextSpan& c) { return c.end_offset; }
  static uint32_t underline_color(const ui::ImeTextSpan& c) {
    return c.underline_color;
  }
  static ui::ImeTextSpan::Thickness thickness(const ui::ImeTextSpan& i) {
    return i.thickness;
  }
  static ui::ImeTextSpan::UnderlineStyle underline_style(
      const ui::ImeTextSpan& i) {
    return i.underline_style;
  }
  static uint32_t text_color(const ui::ImeTextSpan& c) { return c.text_color; }
  static uint32_t background_color(const ui::ImeTextSpan& c) {
    return c.background_color;
  }
  static uint32_t suggestion_highlight_color(const ui::ImeTextSpan& c) {
    return c.suggestion_highlight_color;
  }
  static bool remove_on_finish_composing(const ui::ImeTextSpan& c) {
    return c.remove_on_finish_composing;
  }
  static bool interim_char_selection(const ui::ImeTextSpan& c) {
    return c.interim_char_selection;
  }
  static std::vector<std::string> suggestions(const ui::ImeTextSpan& c) {
    return c.suggestions;
  }
  static bool Read(ui::mojom::ImeTextSpanDataView data, ui::ImeTextSpan* out);
};

template <>
struct COMPONENT_EXPORT(IME_SHARED_MOJOM_TRAITS)
    EnumTraits<ui::mojom::ImeTextSpanType, ui::ImeTextSpan::Type> {
  static ui::mojom::ImeTextSpanType ToMojom(
      ui::ImeTextSpan::Type ime_text_span_type);
  static bool FromMojom(ui::mojom::ImeTextSpanType input,
                        ui::ImeTextSpan::Type* out);
};

template <>
struct COMPONENT_EXPORT(IME_SHARED_MOJOM_TRAITS)
    EnumTraits<ui::mojom::ImeTextSpanThickness, ui::ImeTextSpan::Thickness> {
  static ui::mojom::ImeTextSpanThickness ToMojom(
      ui::ImeTextSpan::Thickness thickness);
  static bool FromMojom(ui::mojom::ImeTextSpanThickness input,
                        ui::ImeTextSpan::Thickness* out);
};

template <>
struct COMPONENT_EXPORT(IME_SHARED_MOJOM_TRAITS)
    EnumTraits<ui::mojom::ImeTextSpanUnderlineStyle,
               ui::ImeTextSpan::UnderlineStyle> {
  static ui::mojom::ImeTextSpanUnderlineStyle ToMojom(
      ui::ImeTextSpan::UnderlineStyle underline_style);
  static bool FromMojom(ui::mojom::ImeTextSpanUnderlineStyle input,
                        ui::ImeTextSpan::UnderlineStyle* out);
};

}  // namespace mojo

#endif  // UI_BASE_IME_MOJOM_IME_TYPES_MOJOM_TRAITS_H_
