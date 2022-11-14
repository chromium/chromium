// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is designed to be included from wayland_*_input_types.cc only,
// in order to share the mapping rules.
// So please do not use in other places.

#ifndef UI_BASE_WAYLAND_WAYLAND_INPUT_TYPES_IMPL_H_
#define UI_BASE_WAYLAND_WAYLAND_INPUT_TYPES_IMPL_H_

// List all TextInputType here.
// This is used to create a mapping between TextInputType and Wayland's
// zcr_extended_text_input_v1_input_type.
// If a new entry is added to TextInputType, then corresponding code
// needs to be added to the wayland's extension, ozone/wayland (wayland client),
// and components/exo (wayland compositor).
#define MAP_TYPES(macro)                                                      \
  macro(NONE) macro(TEXT) macro(PASSWORD) macro(SEARCH) macro(EMAIL)          \
      macro(NUMBER) macro(TELEPHONE) macro(URL) macro(DATE) macro(DATE_TIME)  \
          macro(DATE_TIME_LOCAL) macro(MONTH) macro(TIME) macro(WEEK)         \
              macro(TEXT_AREA) macro(CONTENT_EDITABLE) macro(DATE_TIME_FIELD) \
                  macro(NULL)

// Similar to MAP_TYPES, list all TextInputMode. See above how to update.
#define MAP_MODES(macro)                                                    \
  macro(DEFAULT) macro(NONE) macro(TEXT) macro(TEL) macro(URL) macro(EMAIL) \
      macro(NUMERIC) macro(DECIMAL) macro(SEARCH)

// Similar to MAP_TYPES, list all TextInputFlags. See above how to update.
#define MAP_FLAGS(macro)                                                   \
  macro(NONE) macro(AUTOCOMPLETE_ON) macro(AUTOCOMPLETE_OFF)               \
      macro(AUTOCORRECT_ON) macro(AUTOCORRECT_OFF) macro(SPELLCHECK_ON)    \
          macro(SPELLCHECK_OFF) macro(AUTOCAPITALIZE_NONE)                 \
              macro(AUTOCAPITALIZE_CHARACTERS) macro(AUTOCAPITALIZE_WORDS) \
                  macro(AUTOCAPITALIZE_SENTENCES) macro(HAS_BEEN_PASSWORD) \
                      macro(VERTICAL)

// Note: we assume that the source file including this header implementation
// already includes either text-input-extension-unstable-v1-server-protocol.h
// or text-input-extension-unstable-v1-client-protocol.h.
static inline uint32_t ConvertFromTextInputFlag(ui::TextInputFlags flag) {
  switch (flag) {
#define MAP_ENTRY(name)            \
  case ui::TEXT_INPUT_FLAG_##name: \
    return ZCR_EXTENDED_TEXT_INPUT_V1_INPUT_FLAGS_##name;

    MAP_FLAGS(MAP_ENTRY)
#undef MAP_ENTRY
  }
}

static inline constexpr ui::TextInputFlags kAllTextInputFlags[] = {
#define MAP_ENTRY(name) ui::TEXT_INPUT_FLAG_##name,
    MAP_FLAGS(MAP_ENTRY)
#undef MAP_ENTRY
};

#endif  // UI_BASE_WAYLAND_WAYLAND_INPUT_TYPES_IMPL_H_
