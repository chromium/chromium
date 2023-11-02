// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_zwp_text_input.h"

namespace wl {

namespace {

void TextInputV1Activate(wl_client* client,
                         wl_resource* resource,
                         wl_resource* seat,
                         wl_resource* surface) {
  GetUserDataAs<MockZwpTextInput>(resource)->Activate(surface);
}

void TextInputV1Deactivate(wl_client* client,
                           wl_resource* resource,
                           wl_resource* seat) {
  GetUserDataAs<MockZwpTextInput>(resource)->Deactivate();
}

void TextInputV1ShowInputPanel(wl_client* client, wl_resource* resource) {
  GetUserDataAs<MockZwpTextInput>(resource)->ShowInputPanel();
}

void TextInputV1HideInputPanel(wl_client* client, wl_resource* resource) {
  GetUserDataAs<MockZwpTextInput>(resource)->HideInputPanel();
}

void TextInputV1Reset(wl_client* client, wl_resource* resource) {
  GetUserDataAs<MockZwpTextInput>(resource)->Reset();
}

void TextInputV1SetSurroundingText(wl_client* client,
                                   wl_resource* resource,
                                   const char* text,
                                   uint32_t cursor,
                                   uint32_t anchor) {
  GetUserDataAs<MockZwpTextInput>(resource)->SetSurroundingText(
      text, gfx::Range(cursor, anchor));
}

void TextInputV1SetContentType(wl_client* client,
                               wl_resource* resource,
                               uint32_t content_hint,
                               uint32_t content_purpose) {
  GetUserDataAs<MockZwpTextInput>(resource)->SetContentType(content_hint,
                                                            content_purpose);
}

void TextInputV1SetCursorRectangle(wl_client* client,
                                   wl_resource* resource,
                                   int32_t x,
                                   int32_t y,
                                   int32_t width,
                                   int32_t height) {
  GetUserDataAs<MockZwpTextInput>(resource)->SetCursorRect(x, y, width, height);
}

}  // namespace

const struct zwp_text_input_v1_interface kMockZwpTextInputV1Impl = {
    &TextInputV1Activate,            // activate
    &TextInputV1Deactivate,          // deactivate
    &TextInputV1ShowInputPanel,      // show_input_panel
    &TextInputV1HideInputPanel,      // hide_input_panel
    &TextInputV1Reset,               // reset
    &TextInputV1SetSurroundingText,  // set_surrounding_text
    &TextInputV1SetContentType,      // set_content_type
    &TextInputV1SetCursorRectangle,  // set_cursor_rectangle
    nullptr,                         // set_preferred_language
    nullptr,                         // commit_state
    nullptr,                         // invoke_action
};

MockZwpTextInput::MockZwpTextInput(wl_resource* resource)
    : ServerObject(resource) {}

MockZwpTextInput::~MockZwpTextInput() = default;

}  // namespace wl
