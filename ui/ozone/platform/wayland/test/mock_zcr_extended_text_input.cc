// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_zcr_extended_text_input.h"

namespace wl {

namespace {

void DeprecatedSetInputType(wl_client* client,
                            wl_resource* resource,
                            uint32_t input_type,
                            uint32_t input_mode,
                            uint32_t input_flags,
                            uint32_t learning_mode) {
  GetUserDataAs<MockZcrExtendedTextInput>(resource)->DeprecatedSetInputType(
      input_type, input_mode, input_flags, learning_mode);
}

void SetInputType(wl_client* client,
                  wl_resource* resource,
                  uint32_t input_type,
                  uint32_t input_mode,
                  uint32_t input_flags,
                  uint32_t learning_mode,
                  uint32_t inline_composition_support) {
  GetUserDataAs<MockZcrExtendedTextInput>(resource)->SetInputType(
      input_type, input_mode, input_flags, learning_mode,
      inline_composition_support);
}

void SetGrammarFragmentAtCursor(wl_client* client,
                                wl_resource* resource,
                                uint32_t start,
                                uint32_t end,
                                const char* suggestion) {
  GetUserDataAs<MockZcrExtendedTextInput>(resource)->SetGrammarFragmentAtCursor(
      gfx::Range(start, end), suggestion);
}

void SetAutocorrectInfo(wl_client* client,
                        wl_resource* resource,
                        uint32_t start,
                        uint32_t end,
                        uint32_t x,
                        uint32_t y,
                        uint32_t width,
                        uint32_t height) {
  GetUserDataAs<MockZcrExtendedTextInput>(resource)->SetAutocorrectInfo(
      gfx::Range(start, end), gfx::Rect(x, y, width, height));
}

void FinalizeVirtualKeyboardChanges(wl_client* client, wl_resource* resource) {
  GetUserDataAs<MockZcrExtendedTextInput>(resource)
      ->FinalizeVirtualKeyboardChanges();
}

void SetFocusReason(wl_client* client, wl_resource* resource, uint32_t reason) {
  GetUserDataAs<MockZcrExtendedTextInput>(resource)->SetFocusReason(reason);
}

void SetSurroundingTextSupport(wl_client* client,
                               wl_resource* resource,
                               uint32_t support) {
  GetUserDataAs<MockZcrExtendedTextInput>(resource)->SetSurroundingTextSupport(
      support);
}

void SetSurroundingTextOffsetUtf16(wl_client* client,
                                   wl_resource* resource,
                                   uint32_t offset) {
  GetUserDataAs<MockZcrExtendedTextInput>(resource)
      ->SetSurroundingTextOffsetUtf16(offset);
}

}  // namespace

const struct zcr_extended_text_input_v1_interface
    kMockZcrExtendedTextInputV1Impl = {
        &DestroyResource,                 // destroy
        &DeprecatedSetInputType,          // deprecated_set_input_type
        &SetGrammarFragmentAtCursor,      // set_grammar_fragment_at_cursor
        &SetAutocorrectInfo,              // set_autocorrect_info
        &FinalizeVirtualKeyboardChanges,  // finalize_virtual_keyboard_changes
        &SetFocusReason,                  // set_focus_reason
        &SetInputType,                    // set_input_type
        &SetSurroundingTextSupport,       // set_surrounding_text_support
        &SetSurroundingTextOffsetUtf16,   // set_surrounding_text_offset_utf16
};

MockZcrExtendedTextInput::MockZcrExtendedTextInput(wl_resource* resource)
    : ServerObject(resource) {}

MockZcrExtendedTextInput::~MockZcrExtendedTextInput() = default;

}  // namespace wl
