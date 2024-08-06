// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/mojom/ime_types_mojom_traits.h"

namespace mojo {

#define UI_TO_MOJO_ACTION_CASE(name) \
  case ui::TextInputAction::name:    \
    return ui::mojom::TextInputAction::name

// static
ui::mojom::TextInputAction
EnumTraits<ui::mojom::TextInputAction, ui::TextInputAction>::ToMojom(
    ui::TextInputAction text_input_action) {
  switch (text_input_action) {
    UI_TO_MOJO_ACTION_CASE(kDefault);
    UI_TO_MOJO_ACTION_CASE(kEnter);
    UI_TO_MOJO_ACTION_CASE(kDone);
    UI_TO_MOJO_ACTION_CASE(kGo);
    UI_TO_MOJO_ACTION_CASE(kNext);
    UI_TO_MOJO_ACTION_CASE(kPrevious);
    UI_TO_MOJO_ACTION_CASE(kSearch);
    UI_TO_MOJO_ACTION_CASE(kSend);
  }
}

#undef UI_TO_MOJO_ACTION_CASE

#define MOJO_TO_UI_ACTION_CASE(name)     \
  case ui::mojom::TextInputAction::name: \
    *out = ui::TextInputAction::name;    \
    return true;

// static
bool EnumTraits<ui::mojom::TextInputAction, ui::TextInputAction>::FromMojom(
    ui::mojom::TextInputAction input,
    ui::TextInputAction* out) {
  switch (input) {
    MOJO_TO_UI_ACTION_CASE(kDefault);
    MOJO_TO_UI_ACTION_CASE(kEnter);
    MOJO_TO_UI_ACTION_CASE(kDone);
    MOJO_TO_UI_ACTION_CASE(kGo);
    MOJO_TO_UI_ACTION_CASE(kNext);
    MOJO_TO_UI_ACTION_CASE(kPrevious);
    MOJO_TO_UI_ACTION_CASE(kSearch);
    MOJO_TO_UI_ACTION_CASE(kSend);
  }
  return false;
}

#undef MOJO_TO_UI_ACTION_CASE

#define UI_TO_MOJO_MODE_CASE(name, mojo_name)     \
  case ui::TextInputMode::TEXT_INPUT_MODE_##name: \
    return ui::mojom::TextInputMode::mojo_name

// static
ui::mojom::TextInputMode
EnumTraits<ui::mojom::TextInputMode, ui::TextInputMode>::ToMojom(
    ui::TextInputMode text_input_mode) {
  switch (text_input_mode) {
    UI_TO_MOJO_MODE_CASE(DEFAULT, kDefault);
    UI_TO_MOJO_MODE_CASE(NONE, kNone);
    UI_TO_MOJO_MODE_CASE(TEXT, kText);
    UI_TO_MOJO_MODE_CASE(TEL, kTel);
    UI_TO_MOJO_MODE_CASE(URL, kUrl);
    UI_TO_MOJO_MODE_CASE(EMAIL, kEmail);
    UI_TO_MOJO_MODE_CASE(NUMERIC, kNumeric);
    UI_TO_MOJO_MODE_CASE(DECIMAL, kDecimal);
    UI_TO_MOJO_MODE_CASE(SEARCH, kSearch);
  }
}

#undef UI_TO_MOJO_MODE_CASE

#define MOJO_TO_UI_MODE_CASE(name, mojo_name)         \
  case ui::mojom::TextInputMode::mojo_name:           \
    *out = ui::TextInputMode::TEXT_INPUT_MODE_##name; \
    return true;

// static
bool EnumTraits<ui::mojom::TextInputMode, ui::TextInputMode>::FromMojom(
    ui::mojom::TextInputMode input,
    ui::TextInputMode* out) {
  switch (input) {
    MOJO_TO_UI_MODE_CASE(DEFAULT, kDefault);
    MOJO_TO_UI_MODE_CASE(NONE, kNone);
    MOJO_TO_UI_MODE_CASE(TEXT, kText);
    MOJO_TO_UI_MODE_CASE(TEL, kTel);
    MOJO_TO_UI_MODE_CASE(URL, kUrl);
    MOJO_TO_UI_MODE_CASE(EMAIL, kEmail);
    MOJO_TO_UI_MODE_CASE(NUMERIC, kNumeric);
    MOJO_TO_UI_MODE_CASE(DECIMAL, kDecimal);
    MOJO_TO_UI_MODE_CASE(SEARCH, kSearch);
  }
}

#undef MOJO_TO_UI_MODE_CASE

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
    // Unfortunately we cannot use the macro due to the definition conflict.
    case ui::TEXT_INPUT_TYPE_NULL:
      return ui::mojom::TextInputType::TYPE_NULL;
  }
  NOTREACHED();
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
    // Unfortunately we cannot use the macro due to the definition conflict.
    case ui::mojom::TextInputType::TYPE_NULL:
      *out = ui::TEXT_INPUT_TYPE_NULL;
      return true;
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
  if (!data.ReadUnderlineStyle(&out->underline_style))
    return false;
  out->text_color = data.text_color();
  out->background_color = data.background_color();
  out->suggestion_highlight_color = data.suggestion_highlight_color();
  out->remove_on_finish_composing = data.remove_on_finish_composing();
  out->interim_char_selection = data.interim_char_selection();
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
    case ui::ImeTextSpan::Type::kAutocorrect:
      return ui::mojom::ImeTextSpanType::kAutocorrect;
    case ui::ImeTextSpan::Type::kGrammarSuggestion:
      return ui::mojom::ImeTextSpanType::kGrammarSuggestion;
  }

  NOTREACHED();
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
    case ui::mojom::ImeTextSpanType::kAutocorrect:
      *out = ui::ImeTextSpan::Type::kAutocorrect;
      return true;
    case ui::mojom::ImeTextSpanType::kGrammarSuggestion:
      *out = ui::ImeTextSpan::Type::kGrammarSuggestion;
      return true;
  }

  NOTREACHED();
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
}

// static
ui::mojom::ImeTextSpanUnderlineStyle EnumTraits<
    ui::mojom::ImeTextSpanUnderlineStyle,
    ui::ImeTextSpan::UnderlineStyle>::ToMojom(ui::ImeTextSpan::UnderlineStyle
                                                  underline_style) {
  switch (underline_style) {
    case ui::ImeTextSpan::UnderlineStyle::kNone:
      return ui::mojom::ImeTextSpanUnderlineStyle::kNone;
    case ui::ImeTextSpan::UnderlineStyle::kSolid:
      return ui::mojom::ImeTextSpanUnderlineStyle::kSolid;
    case ui::ImeTextSpan::UnderlineStyle::kDot:
      return ui::mojom::ImeTextSpanUnderlineStyle::kDot;
    case ui::ImeTextSpan::UnderlineStyle::kDash:
      return ui::mojom::ImeTextSpanUnderlineStyle::kDash;
    case ui::ImeTextSpan::UnderlineStyle::kSquiggle:
      return ui::mojom::ImeTextSpanUnderlineStyle::kSquiggle;
  }

  NOTREACHED();
}

// static
bool EnumTraits<ui::mojom::ImeTextSpanUnderlineStyle,
                ui::ImeTextSpan::UnderlineStyle>::
    FromMojom(ui::mojom::ImeTextSpanUnderlineStyle input,
              ui::ImeTextSpan::UnderlineStyle* out) {
  switch (input) {
    case ui::mojom::ImeTextSpanUnderlineStyle::kNone:
      *out = ui::ImeTextSpan::UnderlineStyle::kNone;
      return true;
    case ui::mojom::ImeTextSpanUnderlineStyle::kSolid:
      *out = ui::ImeTextSpan::UnderlineStyle::kSolid;
      return true;
    case ui::mojom::ImeTextSpanUnderlineStyle::kDot:
      *out = ui::ImeTextSpan::UnderlineStyle::kDot;
      return true;
    case ui::mojom::ImeTextSpanUnderlineStyle::kDash:
      *out = ui::ImeTextSpan::UnderlineStyle::kDash;
      return true;
    case ui::mojom::ImeTextSpanUnderlineStyle::kSquiggle:
      *out = ui::ImeTextSpan::UnderlineStyle::kSquiggle;
      return true;
  }

  NOTREACHED();
}

}  // namespace mojo
