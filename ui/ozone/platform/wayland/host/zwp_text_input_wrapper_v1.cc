// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/zwp_text_input_wrapper_v1.h"

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/range/range.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

ZWPTextInputWrapperV1::ZWPTextInputWrapperV1(
    zwp_text_input_manager_v1* text_input_manager)
    : client_(nullptr) {
  static const zwp_text_input_v1_listener text_input_listener = {
      &ZWPTextInputWrapperV1::OnEnter,         // text_input_enter,
      &ZWPTextInputWrapperV1::OnLeave,         // text_input_leave,
      &ZWPTextInputWrapperV1::OnModifiersMap,  // text_input_modifiers_map,
      &ZWPTextInputWrapperV1::
          OnInputPanelState,                    // text_input_input_panel_state,
      &ZWPTextInputWrapperV1::OnPreeditString,  // text_input_preedit_string,
      &ZWPTextInputWrapperV1::OnPreeditStyling,  // text_input_preedit_styling,
      &ZWPTextInputWrapperV1::OnPreeditCursor,   // text_input_preedit_cursor,
      &ZWPTextInputWrapperV1::OnCommitString,    // text_input_commit_string,
      &ZWPTextInputWrapperV1::OnCursorPosition,  // text_input_cursor_position,
      &ZWPTextInputWrapperV1::
          OnDeleteSurroundingText,       // text_input_delete_surrounding_text,
      &ZWPTextInputWrapperV1::OnKeysym,  // text_input_keysym,
      &ZWPTextInputWrapperV1::OnLanguage,       // text_input_language,
      &ZWPTextInputWrapperV1::OnTextDirection,  // text_input_text_direction
  };

  zwp_text_input_v1* text_input =
      zwp_text_input_manager_v1_create_text_input(text_input_manager);
  obj_ = wl::Object<zwp_text_input_v1>(text_input);

  zwp_text_input_v1_add_listener(text_input, &text_input_listener, this);
}

ZWPTextInputWrapperV1::~ZWPTextInputWrapperV1() {}

void ZWPTextInputWrapperV1::Initialize(WaylandConnection* connection,
                                       ZWPTextInputWrapperClient* client) {
  connection_ = connection;
  client_ = client;
}

void ZWPTextInputWrapperV1::Reset() {
  ResetInputEventState();
  zwp_text_input_v1_reset(obj_.get());
}

void ZWPTextInputWrapperV1::Activate(WaylandWindow* window) {
  zwp_text_input_v1_activate(obj_.get(), connection_->seat(),
                             window->root_surface()->surface());
}

void ZWPTextInputWrapperV1::Deactivate() {
  zwp_text_input_v1_deactivate(obj_.get(), connection_->seat());
}

void ZWPTextInputWrapperV1::ShowInputPanel() {
  zwp_text_input_v1_show_input_panel(obj_.get());
}

void ZWPTextInputWrapperV1::HideInputPanel() {
  zwp_text_input_v1_hide_input_panel(obj_.get());
}

void ZWPTextInputWrapperV1::SetCursorRect(const gfx::Rect& rect) {
  zwp_text_input_v1_set_cursor_rectangle(obj_.get(), rect.x(), rect.y(),
                                         rect.width(), rect.height());
}

void ZWPTextInputWrapperV1::SetSurroundingText(
    const base::string16& text,
    const gfx::Range& selection_range) {
  static constexpr size_t kWaylandMessageDataMaxLength = 4000;
  const std::string text_utf8 = base::UTF16ToUTF8(text);
  // The text length for set_surrounding_text can not be longer than the maximum
  // length of wayland messages. The maximum length of the text is explicitly
  // specified as 4000 in the protocol spec of text-input-unstable-v3.
  // If the client is unware of the text around the cursor, we can skip sending
  // set_surrounding_text requests. We fall back to this case when the text is
  // too long.
  // TODO(fukino): If the length of |text| doesn't fit into the 4000 bytes
  // limitation, we should truncate the text and adjust indices of
  // |selection_range| to make use of set_surrounding_text as much as possible.
  // crbug.com/1173465.
  if (text_utf8.size() > kWaylandMessageDataMaxLength)
    return;

  zwp_text_input_v1_set_surrounding_text(obj_.get(), text_utf8.c_str(),
                                         selection_range.start(),
                                         selection_range.end());
}

void ZWPTextInputWrapperV1::ResetInputEventState() {
  spans_.clear();
  preedit_cursor_ = -1;
}

void ZWPTextInputWrapperV1::OnEnter(void* data,
                                    struct zwp_text_input_v1* text_input,
                                    struct wl_surface* surface) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ZWPTextInputWrapperV1::OnLeave(void* data,
                                    struct zwp_text_input_v1* text_input) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ZWPTextInputWrapperV1::OnModifiersMap(void* data,
                                           struct zwp_text_input_v1* text_input,
                                           struct wl_array* map) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ZWPTextInputWrapperV1::OnInputPanelState(
    void* data,
    struct zwp_text_input_v1* text_input,
    uint32_t state) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ZWPTextInputWrapperV1::OnPreeditString(
    void* data,
    struct zwp_text_input_v1* text_input,
    uint32_t serial,
    const char* text,
    const char* commit) {
  ZWPTextInputWrapperV1* wti = static_cast<ZWPTextInputWrapperV1*>(data);
  auto spans = std::move(wti->spans_);
  int32_t preedit_cursor = wti->preedit_cursor_;
  wti->ResetInputEventState();
  wti->client_->OnPreeditString(text, spans, preedit_cursor);
}

void ZWPTextInputWrapperV1::OnPreeditStyling(
    void* data,
    struct zwp_text_input_v1* text_input,
    uint32_t index,
    uint32_t length,
    uint32_t style) {
  ZWPTextInputWrapperV1* wti = static_cast<ZWPTextInputWrapperV1*>(data);
  wti->spans_.push_back(
      ZWPTextInputWrapperClient::SpanStyle{index, length, style});
}

void ZWPTextInputWrapperV1::OnPreeditCursor(
    void* data,
    struct zwp_text_input_v1* text_input,
    int32_t index) {
  ZWPTextInputWrapperV1* wti = static_cast<ZWPTextInputWrapperV1*>(data);
  wti->preedit_cursor_ = index;
}

void ZWPTextInputWrapperV1::OnCommitString(void* data,
                                           struct zwp_text_input_v1* text_input,
                                           uint32_t serial,
                                           const char* text) {
  ZWPTextInputWrapperV1* wti = static_cast<ZWPTextInputWrapperV1*>(data);
  wti->ResetInputEventState();
  wti->client_->OnCommitString(text);
}

void ZWPTextInputWrapperV1::OnCursorPosition(
    void* data,
    struct zwp_text_input_v1* text_input,
    int32_t index,
    int32_t anchor) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ZWPTextInputWrapperV1::OnDeleteSurroundingText(
    void* data,
    struct zwp_text_input_v1* text_input,
    int32_t index,
    uint32_t length) {
  ZWPTextInputWrapperV1* wti = static_cast<ZWPTextInputWrapperV1*>(data);
  wti->client_->OnDeleteSurroundingText(index, length);
}

void ZWPTextInputWrapperV1::OnKeysym(void* data,
                                     struct zwp_text_input_v1* text_input,
                                     uint32_t serial,
                                     uint32_t time,
                                     uint32_t key,
                                     uint32_t state,
                                     uint32_t modifiers) {
  ZWPTextInputWrapperV1* wti = static_cast<ZWPTextInputWrapperV1*>(data);
  wti->client_->OnKeysym(key, state, modifiers);
}

void ZWPTextInputWrapperV1::OnLanguage(void* data,
                                       struct zwp_text_input_v1* text_input,
                                       uint32_t serial,
                                       const char* language) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ZWPTextInputWrapperV1::OnTextDirection(
    void* data,
    struct zwp_text_input_v1* text_input,
    uint32_t serial,
    uint32_t direction) {
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace ui
