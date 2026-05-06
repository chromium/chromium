// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_TEXT_INPUT_FLAGS_H_
#define UI_BASE_IME_TEXT_INPUT_FLAGS_H_

#include "build/build_config.h"

namespace ui {

// Intentionally keep in sync with blink::WebTextInputFlags defined in:
// third_party/blink/public/platform/web_text_input_type.h
// LINT.IfChange(TextInputFlags)
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
// Used to enable/disable the "Previous"/"Next" buttons in the iOS keyboard
// input accessory toolbar. Android passes these bits as a raw integer to Java
// and never decodes them via this enum; desktop has no such toolbar UI.
#if BUILDFLAG(IS_IOS)
  TEXT_INPUT_FLAG_HAVE_NEXT_FOCUSABLE_ELEMENT = 1 << 10,
  TEXT_INPUT_FLAG_HAVE_PREVIOUS_FOCUSABLE_ELEMENT = 1 << 11,
#endif
  TEXT_INPUT_FLAG_HAS_BEEN_PASSWORD = 1 << 12,
  TEXT_INPUT_FLAG_VERTICAL = 1 << 13,
  // Whether an input field is or has been a custom password field. This is a
  // best effort heuristic to determine what a "password" is based on the
  // field's behavior.
  TEXT_INPUT_FLAG_HAS_BEEN_CUSTOM_PASSWORD = 1 << 14
};
// LINT.ThenChange(//third_party/blink/public/platform/web_text_input_type.h:WebTextInputFlags)

}  // namespace ui

#endif  // UI_BASE_IME_TEXT_INPUT_FLAGS_H_
