// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_TEXT_INPUT_FLAGS_H_
#define UI_BASE_IME_TEXT_INPUT_FLAGS_H_

namespace ui {

// Intentionally keep in sync with blink::WebTextInputFlags defined in:
// third_party/blink/public/platform/web_text_input_type.h
enum TextInputFlags {
  TEXT_INPUT_FLAG_NONE = 0,
  TEXT_INPUT_FLAG_AUTOCOMPLETE_ON = 1 << 0,
  TEXT_INPUT_FLAG_AUTOCOMPLETE_OFF = 1 << 1,
  TEXT_INPUT_FLAG_AUTOCORRECT_ON = 1 << 2,
  TEXT_INPUT_FLAG_AUTOCORRECT_OFF = 1 << 3,
  TEXT_INPUT_FLAG_SPELLCHECK_ON = 1 << 4,
  TEXT_INPUT_FLAG_SPELLCHECK_OFF = 1 << 5,
  TEXT_INPUT_FLAG_AUTOCAPITALIZE_NONE = 1 << 6,
  TEXT_INPUT_FLAG_AUTOCAPITALIZE_CHARACTERS = 1 << 7,
  TEXT_INPUT_FLAG_AUTOCAPITALIZE_WORDS = 1 << 8,
  TEXT_INPUT_FLAG_AUTOCAPITALIZE_SENTENCES = 1 << 9,
  TEXT_INPUT_FLAG_HAS_BEEN_PASSWORD = 1 << 12,
  TEXT_INPUT_FLAG_VERTICAL = 1 << 13
};

}  // namespace ui

#endif  // UI_BASE_IME_TEXT_INPUT_FLAGS_H_
