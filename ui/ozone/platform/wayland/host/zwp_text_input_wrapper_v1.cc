// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/zwp_text_input_wrapper_v1.h"

#include <string>
#include <utility>

#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "ui/base/wayland/wayland_client_input_types.h"
#include "ui/gfx/range/range.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {
namespace {

// Converts Chrome's TextInputType into wayland's content_purpose.
// Some of TextInputType values do not have clearly corresponding wayland value,
// and they fallback to closer type.
uint32_t InputTypeToContentPurpose(TextInputType input_type) {
  switch (input_type) {
    case TEXT_INPUT_TYPE_NONE:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NORMAL;
    case TEXT_INPUT_TYPE_TEXT:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NORMAL;
    case TEXT_INPUT_TYPE_PASSWORD:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_PASSWORD;
    case TEXT_INPUT_TYPE_SEARCH:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NORMAL;
    case TEXT_INPUT_TYPE_EMAIL:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_EMAIL;
    case TEXT_INPUT_TYPE_NUMBER:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NUMBER;
    case TEXT_INPUT_TYPE_TELEPHONE:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_PHONE;
    case TEXT_INPUT_TYPE_URL:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_URL;
    case TEXT_INPUT_TYPE_DATE:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_DATE;
    case TEXT_INPUT_TYPE_DATE_TIME:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_DATETIME;
    case TEXT_INPUT_TYPE_DATE_TIME_LOCAL:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_DATETIME;
    case TEXT_INPUT_TYPE_MONTH:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_DATE;
    case TEXT_INPUT_TYPE_TIME:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_TIME;
    case TEXT_INPUT_TYPE_WEEK:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_DATE;
    case TEXT_INPUT_TYPE_TEXT_AREA:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NORMAL;
    case TEXT_INPUT_TYPE_CONTENT_EDITABLE:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NORMAL;
    case TEXT_INPUT_TYPE_DATE_TIME_FIELD:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_DATETIME;
    case TEXT_INPUT_TYPE_NULL:
      return ZWP_TEXT_INPUT_V1_CONTENT_PURPOSE_NORMAL;
  }
}

// Converts Chrome's TextInputType into wayland's content_hint.
uint32_t InputFlagsToContentHint(int input_flags) {
  uint32_t hint = 0;
  if (input_flags & TEXT_INPUT_FLAG_AUTOCOMPLETE_ON)
    hint |= ZWP_TEXT_INPUT_V1_CONTENT_HINT_AUTO_COMPLETION;
  if (input_flags & TEXT_INPUT_FLAG_AUTOCORRECT_ON)
    hint |= ZWP_TEXT_INPUT_V1_CONTENT_HINT_AUTO_CORRECTION;
  // No good match. Fallback to AUTO_CORRECTION.
  if (input_flags & TEXT_INPUT_FLAG_SPELLCHECK_ON)
    hint |= ZWP_TEXT_INPUT_V1_CONTENT_HINT_AUTO_CORRECTION;
  if (input_flags & TEXT_INPUT_FLAG_AUTOCAPITALIZE_CHARACTERS)
    hint |= ZWP_TEXT_INPUT_V1_CONTENT_HINT_UPPERCASE;
  if (input_flags & TEXT_INPUT_FLAG_AUTOCAPITALIZE_WORDS)
    hint |= ZWP_TEXT_INPUT_V1_CONTENT_HINT_TITLECASE;
  if (input_flags & TEXT_INPUT_FLAG_AUTOCAPITALIZE_SENTENCES)
    hint |= ZWP_TEXT_INPUT_V1_CONTENT_HINT_AUTO_CAPITALIZATION;
  if (input_flags & TEXT_INPUT_FLAG_HAS_BEEN_PASSWORD)
    hint |= ZWP_TEXT_INPUT_V1_CONTENT_HINT_PASSWORD;
  return hint;
}

// Parses the content of |array|, and creates a map of modifiers.
// The content of array is just a concat of modifier names in c-style string
// (i.e., '\0' terminated string), thus this splits the whole byte array by
// '\0' character.
std::vector<std::string> ParseModifiersMap(wl_array* array) {
  return base::SplitString(
      base::StringPiece(static_cast<char*>(array->data),
                        array->size - 1),  // exclude trailing '\0'.
      base::StringPiece("\0", 1),          // '\0' as a delimiter.
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
}

}  // namespace

ZWPTextInputWrapperV1::ZWPTextInputWrapperV1(
    WaylandConnection* connection,
    ZWPTextInputWrapperClient* client,
    zwp_text_input_manager_v1* text_input_manager,
    zcr_text_input_extension_v1* text_input_extension)
    : connection_(connection), client_(client) {
  static constexpr zwp_text_input_v1_listener text_input_listener = {
      &OnEnter,                  // text_input_enter,
      &OnLeave,                  // text_input_leave,
      &OnModifiersMap,           // text_input_modifiers_map,
      &OnInputPanelState,        // text_input_input_panel_state,
      &OnPreeditString,          // text_input_preedit_string,
      &OnPreeditStyling,         // text_input_preedit_styling,
      &OnPreeditCursor,          // text_input_preedit_cursor,
      &OnCommitString,           // text_input_commit_string,
      &OnCursorPosition,         // text_input_cursor_position,
      &OnDeleteSurroundingText,  // text_input_delete_surrounding_text,
      &OnKeysym,                 // text_input_keysym,
      &OnLanguage,               // text_input_language,
      &OnTextDirection,          // text_input_text_direction
  };

  static constexpr zcr_extended_text_input_v1_listener
      extended_text_input_listener = {
          &OnSetPreeditRegion,  // extended_text_input_set_preedit_region,
      };

  auto* text_input =
      zwp_text_input_manager_v1_create_text_input(text_input_manager);
  obj_ = wl::Object<zwp_text_input_v1>(text_input);
  zwp_text_input_v1_add_listener(text_input, &text_input_listener, this);

  if (text_input_extension) {
    auto* extended_text_input =
        zcr_text_input_extension_v1_get_extended_text_input(
            text_input_extension, obj_.get());
    if (extended_text_input) {
      extended_obj_ =
          wl::Object<zcr_extended_text_input_v1>(extended_text_input);
      zcr_extended_text_input_v1_add_listener(
          extended_text_input, &extended_text_input_listener, this);
    }
  }
}

ZWPTextInputWrapperV1::~ZWPTextInputWrapperV1() = default;

void ZWPTextInputWrapperV1::Reset() {
  ResetInputEventState();
  zwp_text_input_v1_reset(obj_.get());
}

void ZWPTextInputWrapperV1::Activate(WaylandWindow* window) {
  DCHECK(connection_->seat());

  zwp_text_input_v1_activate(obj_.get(), connection_->seat()->wl_object(),
                             window->root_surface()->surface());
}

void ZWPTextInputWrapperV1::Deactivate() {
  DCHECK(connection_->seat());

  zwp_text_input_v1_deactivate(obj_.get(), connection_->seat()->wl_object());
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
    const std::string& text,
    const gfx::Range& selection_range) {
  zwp_text_input_v1_set_surrounding_text(
      obj_.get(), text.c_str(), selection_range.start(), selection_range.end());
}

void ZWPTextInputWrapperV1::SetContentType(ui::TextInputType type,
                                           ui::TextInputMode mode,
                                           uint32_t flags,
                                           bool should_do_learning) {
  // If wayland compositor supports the extended version of set input type,
  // use it to avoid loosing the info.
  if (extended_obj_.get() &&
      wl::get_version_of_object(extended_obj_.get()) >=
          ZCR_EXTENDED_TEXT_INPUT_V1_SET_INPUT_TYPE_SINCE_VERSION) {
    zcr_extended_text_input_v1_set_input_type(
        extended_obj_.get(), ui::wayland::ConvertFromTextInputType(type),
        ui::wayland::ConvertFromTextInputMode(mode),
        ui::wayland::ConvertFromTextInputFlags(flags),
        should_do_learning ? ZCR_EXTENDED_TEXT_INPUT_V1_LEARNING_MODE_ENABLED
                           : ZCR_EXTENDED_TEXT_INPUT_V1_LEARNING_MODE_DISABLED);
    return;
  }

  // Otherwise, fallback to the standard set_content_type.
  uint32_t content_purpose = InputTypeToContentPurpose(type);
  uint32_t content_hint = InputFlagsToContentHint(flags);
  if (!should_do_learning)
    content_hint |= ZWP_TEXT_INPUT_V1_CONTENT_HINT_SENSITIVE_DATA;
  zwp_text_input_v1_set_content_type(obj_.get(), content_hint, content_purpose);
}

void ZWPTextInputWrapperV1::ResetInputEventState() {
  spans_.clear();
  preedit_cursor_ = -1;
}

// static
void ZWPTextInputWrapperV1::OnEnter(void* data,
                                    struct zwp_text_input_v1* text_input,
                                    struct wl_surface* surface) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void ZWPTextInputWrapperV1::OnLeave(void* data,
                                    struct zwp_text_input_v1* text_input) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void ZWPTextInputWrapperV1::OnModifiersMap(void* data,
                                           struct zwp_text_input_v1* text_input,
                                           struct wl_array* map) {
  auto* self = static_cast<ZWPTextInputWrapperV1*>(data);
  self->client_->OnModifiersMap(ParseModifiersMap(map));
}

// static
void ZWPTextInputWrapperV1::OnInputPanelState(
    void* data,
    struct zwp_text_input_v1* text_input,
    uint32_t state) {
  auto* self = static_cast<ZWPTextInputWrapperV1*>(data);
  self->client_->OnInputPanelState(state);
}

// static
void ZWPTextInputWrapperV1::OnPreeditString(
    void* data,
    struct zwp_text_input_v1* text_input,
    uint32_t serial,
    const char* text,
    const char* commit) {
  auto* self = static_cast<ZWPTextInputWrapperV1*>(data);
  auto spans = std::move(self->spans_);
  int32_t preedit_cursor = self->preedit_cursor_;
  self->ResetInputEventState();
  self->client_->OnPreeditString(text, spans, preedit_cursor);
}

// static
void ZWPTextInputWrapperV1::OnPreeditStyling(
    void* data,
    struct zwp_text_input_v1* text_input,
    uint32_t index,
    uint32_t length,
    uint32_t style) {
  auto* self = static_cast<ZWPTextInputWrapperV1*>(data);
  self->spans_.push_back(
      ZWPTextInputWrapperClient::SpanStyle{index, length, style});
}

// static
void ZWPTextInputWrapperV1::OnPreeditCursor(
    void* data,
    struct zwp_text_input_v1* text_input,
    int32_t index) {
  auto* self = static_cast<ZWPTextInputWrapperV1*>(data);
  self->preedit_cursor_ = index;
}

// static
void ZWPTextInputWrapperV1::OnCommitString(void* data,
                                           struct zwp_text_input_v1* text_input,
                                           uint32_t serial,
                                           const char* text) {
  auto* self = static_cast<ZWPTextInputWrapperV1*>(data);
  self->ResetInputEventState();
  self->client_->OnCommitString(text);
}

// static
void ZWPTextInputWrapperV1::OnCursorPosition(
    void* data,
    struct zwp_text_input_v1* text_input,
    int32_t index,
    int32_t anchor) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void ZWPTextInputWrapperV1::OnDeleteSurroundingText(
    void* data,
    struct zwp_text_input_v1* text_input,
    int32_t index,
    uint32_t length) {
  auto* self = static_cast<ZWPTextInputWrapperV1*>(data);
  self->client_->OnDeleteSurroundingText(index, length);
}

// static
void ZWPTextInputWrapperV1::OnKeysym(void* data,
                                     struct zwp_text_input_v1* text_input,
                                     uint32_t serial,
                                     uint32_t time,
                                     uint32_t key,
                                     uint32_t state,
                                     uint32_t modifiers) {
  auto* self = static_cast<ZWPTextInputWrapperV1*>(data);
  self->client_->OnKeysym(key, state, modifiers);
}

// static
void ZWPTextInputWrapperV1::OnLanguage(void* data,
                                       struct zwp_text_input_v1* text_input,
                                       uint32_t serial,
                                       const char* language) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void ZWPTextInputWrapperV1::OnTextDirection(
    void* data,
    struct zwp_text_input_v1* text_input,
    uint32_t serial,
    uint32_t direction) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void ZWPTextInputWrapperV1::OnSetPreeditRegion(
    void* data,
    struct zcr_extended_text_input_v1* extended_text_input,
    int32_t index,
    uint32_t length) {
  auto* self = static_cast<ZWPTextInputWrapperV1*>(data);
  auto spans = std::move(self->spans_);
  self->ResetInputEventState();
  self->client_->OnSetPreeditRegion(index, length, spans);
}

}  // namespace ui
