// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/mojom/ime_types_mojom_traits.h"

namespace mojo {

#define UI_TO_MOJO_TYPE_CASE(name) \
  case ui::TEXT_INPUT_TYPE_##name: \
    return ui::mojom::TextInputType::name

// static
ui::mojom::TextInputType
EnumTraits<ui::mojom::TextInputType, ui::TextInputType>::ToMojom(
    ui::TextInputType text_input_type) {
  switch (text_input_type) {
    UI_TO_MOJO_TYPE_CASE(NONE);
    UI_TO_MOJO_TYPE_CASE(TEXT);
    UI_TO_MOJO_TYPE_CASE(PASSWORD);
    UI_TO_MOJO_TYPE_CASE(SEARCH);
    UI_TO_MOJO_TYPE_CASE(EMAIL);
    UI_TO_MOJO_TYPE_CASE(NUMBER);
    UI_TO_MOJO_TYPE_CASE(TELEPHONE);
    UI_TO_MOJO_TYPE_CASE(URL);
    UI_TO_MOJO_TYPE_CASE(DATE);
    UI_TO_MOJO_TYPE_CASE(DATE_TIME);
    UI_TO_MOJO_TYPE_CASE(DATE_TIME_LOCAL);
    UI_TO_MOJO_TYPE_CASE(MONTH);
    UI_TO_MOJO_TYPE_CASE(TIME);
    UI_TO_MOJO_TYPE_CASE(WEEK);
    UI_TO_MOJO_TYPE_CASE(TEXT_AREA);
    UI_TO_MOJO_TYPE_CASE(CONTENT_EDITABLE);
    UI_TO_MOJO_TYPE_CASE(DATE_TIME_FIELD);
  }
  NOTREACHED();
  return ui::mojom::TextInputType::NONE;
}

#undef UI_TO_MOJO_TYPE_CASE

#define MOJO_TO_UI_TYPE_CASE(name)     \
  case ui::mojom::TextInputType::name: \
    *out = ui::TEXT_INPUT_TYPE_##name; \
    return true;

// static
bool EnumTraits<ui::mojom::TextInputType, ui::TextInputType>::FromMojom(
    ui::mojom::TextInputType input,
    ui::TextInputType* out) {
  switch (input) {
    MOJO_TO_UI_TYPE_CASE(NONE);
    MOJO_TO_UI_TYPE_CASE(TEXT);
    MOJO_TO_UI_TYPE_CASE(PASSWORD);
    MOJO_TO_UI_TYPE_CASE(SEARCH);
    MOJO_TO_UI_TYPE_CASE(EMAIL);
    MOJO_TO_UI_TYPE_CASE(NUMBER);
    MOJO_TO_UI_TYPE_CASE(TELEPHONE);
    MOJO_TO_UI_TYPE_CASE(URL);
    MOJO_TO_UI_TYPE_CASE(DATE);
    MOJO_TO_UI_TYPE_CASE(DATE_TIME);
    MOJO_TO_UI_TYPE_CASE(DATE_TIME_LOCAL);
    MOJO_TO_UI_TYPE_CASE(MONTH);
    MOJO_TO_UI_TYPE_CASE(TIME);
    MOJO_TO_UI_TYPE_CASE(WEEK);
    MOJO_TO_UI_TYPE_CASE(TEXT_AREA);
    MOJO_TO_UI_TYPE_CASE(CONTENT_EDITABLE);
    MOJO_TO_UI_TYPE_CASE(DATE_TIME_FIELD);
  }
#undef MOJO_TO_UI_TYPE_CASE
  return false;
}

// static
bool StructTraits<ui::mojom::ImeTextSpanDataView, ui::ImeTextSpan>::Read(
    ui::mojom::ImeTextSpanDataView data,
    ui::ImeTextSpan* out) {
  if (data.is_null())
    return false;
  if (!data.ReadType(&out->type))
    return false;
  out->start_offset = data.start_offset();
  out->end_offset = data.end_offset();
  out->underline_color = data.underline_color();
  if (!data.ReadThickness(&out->thickness))
    return false;
  out->background_color = data.background_color();
  out->suggestion_highlight_color = data.suggestion_highlight_color();
  out->remove_on_finish_composing = data.remove_on_finish_composing();
  if (!data.ReadSuggestions(&out->suggestions))
    return false;
  return true;
}

// static
ui::mojom::ImeTextSpanType
EnumTraits<ui::mojom::ImeTextSpanType, ui::ImeTextSpan::Type>::ToMojom(
    ui::ImeTextSpan::Type ime_text_span_type) {
  switch (ime_text_span_type) {
    case ui::ImeTextSpan::Type::kComposition:
      return ui::mojom::ImeTextSpanType::kComposition;
    case ui::ImeTextSpan::Type::kSuggestion:
      return ui::mojom::ImeTextSpanType::kSuggestion;
    case ui::ImeTextSpan::Type::kMisspellingSuggestion:
      return ui::mojom::ImeTextSpanType::kMisspellingSuggestion;
  }

  NOTREACHED();
  return ui::mojom::ImeTextSpanType::kComposition;
}

// static
bool EnumTraits<ui::mojom::ImeTextSpanType, ui::ImeTextSpan::Type>::FromMojom(
    ui::mojom::ImeTextSpanType type,
    ui::ImeTextSpan::Type* out) {
  switch (type) {
    case ui::mojom::ImeTextSpanType::kComposition:
      *out = ui::ImeTextSpan::Type::kComposition;
      return true;
    case ui::mojom::ImeTextSpanType::kSuggestion:
      *out = ui::ImeTextSpan::Type::kSuggestion;
      return true;
    case ui::mojom::ImeTextSpanType::kMisspellingSuggestion:
      *out = ui::ImeTextSpan::Type::kMisspellingSuggestion;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
ui::mojom::ImeTextSpanThickness EnumTraits<
    ui::mojom::ImeTextSpanThickness,
    ui::ImeTextSpan::Thickness>::ToMojom(ui::ImeTextSpan::Thickness thickness) {
  switch (thickness) {
    case ui::ImeTextSpan::Thickness::kNone:
      return ui::mojom::ImeTextSpanThickness::kNone;
    case ui::ImeTextSpan::Thickness::kThin:
      return ui::mojom::ImeTextSpanThickness::kThin;
    case ui::ImeTextSpan::Thickness::kThick:
      return ui::mojom::ImeTextSpanThickness::kThick;
  }

  NOTREACHED();
  return ui::mojom::ImeTextSpanThickness::kThin;
}

// static
bool EnumTraits<ui::mojom::ImeTextSpanThickness, ui::ImeTextSpan::Thickness>::
    FromMojom(ui::mojom::ImeTextSpanThickness input,
              ui::ImeTextSpan::Thickness* out) {
  switch (input) {
    case ui::mojom::ImeTextSpanThickness::kNone:
      *out = ui::ImeTextSpan::Thickness::kNone;
      return true;
    case ui::mojom::ImeTextSpanThickness::kThin:
      *out = ui::ImeTextSpan::Thickness::kThin;
      return true;
    case ui::mojom::ImeTextSpanThickness::kThick:
      *out = ui::ImeTextSpan::Thickness::kThick;
      return true;
  }

  NOTREACHED();
  return false;
}

}  // namespace mojo
