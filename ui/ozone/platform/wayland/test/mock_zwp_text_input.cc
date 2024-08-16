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
  GetUserDataAs<MockZwpTextInputV1>(resource)->Activate(surface);
}

void TextInputV1Deactivate(wl_client* client,
                           wl_resource* resource,
                           wl_resource* seat) {
  GetUserDataAs<MockZwpTextInputV1>(resource)->Deactivate();
}

void TextInputV1ShowInputPanel(wl_client* client, wl_resource* resource) {
  GetUserDataAs<MockZwpTextInputV1>(resource)->ShowInputPanel();
}

void TextInputV1HideInputPanel(wl_client* client, wl_resource* resource) {
  GetUserDataAs<MockZwpTextInputV1>(resource)->HideInputPanel();
}

void TextInputV1Reset(wl_client* client, wl_resource* resource) {
  GetUserDataAs<MockZwpTextInputV1>(resource)->Reset();
}

void TextInputV1SetSurroundingText(wl_client* client,
                                   wl_resource* resource,
                                   const char* text,
                                   uint32_t cursor,
                                   uint32_t anchor) {
  GetUserDataAs<MockZwpTextInputV1>(resource)->SetSurroundingText(
      text, gfx::Range(cursor, anchor));
}

void TextInputV1SetContentType(wl_client* client,
                               wl_resource* resource,
                               uint32_t content_hint,
                               uint32_t content_purpose) {
  GetUserDataAs<MockZwpTextInputV1>(resource)->SetContentType(content_hint,
                                                              content_purpose);
}

void TextInputV1SetCursorRectangle(wl_client* client,
                                   wl_resource* resource,
                                   int32_t x,
                                   int32_t y,
                                   int32_t width,
                                   int32_t height) {
  GetUserDataAs<MockZwpTextInputV1>(resource)->SetCursorRect(x, y, width,
                                                             height);
}

void TextInputV3Enable(struct wl_client* client, struct wl_resource* resource) {
  GetUserDataAs<MockZwpTextInputV3>(resource)->Enable();
}

void TextInputV3Disable(struct wl_client* client,
                        struct wl_resource* resource) {
  GetUserDataAs<MockZwpTextInputV3>(resource)->Disable();
}

void TextInputV3SetSurroundingText(struct wl_client* client,
                                   struct wl_resource* resource,
                                   const char* text,
                                   int32_t cursor,
                                   int32_t anchor) {
  GetUserDataAs<MockZwpTextInputV3>(resource)->SetSurroundingText(
      text, gfx::Range(anchor, cursor));
}

void TextInputV3SetTextChangeCause(struct wl_client* client,
                                   struct wl_resource* resource,
                                   uint32_t cause) {
  GetUserDataAs<MockZwpTextInputV3>(resource)->SetTextChangeCause(cause);
}

void TextInputV3SetContentType(struct wl_client* client,
                               struct wl_resource* resource,
                               uint32_t hint,
                               uint32_t purpose) {
  GetUserDataAs<MockZwpTextInputV3>(resource)->SetContentType(hint, purpose);
}

void TextInputV3SetCursorRectangle(struct wl_client* client,
                                   struct wl_resource* resource,
                                   int32_t x,
                                   int32_t y,
                                   int32_t width,
                                   int32_t height) {
  GetUserDataAs<MockZwpTextInputV3>(resource)->SetCursorRect(x, y, width,
                                                             height);
}

void TextInputV3Commit(struct wl_client* client, struct wl_resource* resource) {
  GetUserDataAs<MockZwpTextInputV3>(resource)->Commit();
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

const struct zwp_text_input_v3_interface kMockZwpTextInputV3Impl = {
    &DestroyResource,
    &TextInputV3Enable,
    &TextInputV3Disable,
    &TextInputV3SetSurroundingText,
    &TextInputV3SetTextChangeCause,
    &TextInputV3SetContentType,
    &TextInputV3SetCursorRectangle,
    &TextInputV3Commit,
};

MockZwpTextInputV1::MockZwpTextInputV1(wl_resource* resource)
    : ServerObject(resource) {}

MockZwpTextInputV1::~MockZwpTextInputV1() {
  if (text_input_manager_) {
    text_input_manager_->OnTextInputDestroyed(this);
  }
}

MockZwpTextInputV3::MockZwpTextInputV3(wl_resource* resource)
    : ServerObject(resource) {}

MockZwpTextInputV3::~MockZwpTextInputV3() {
  if (text_input_manager_) {
    text_input_manager_->OnTextInputDestroyed(this);
  }
}

}  // namespace wl
