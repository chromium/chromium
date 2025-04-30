// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/zwp_text_input_v1.h"

#include <sys/mman.h>

#include <string>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
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
  if (input_flags & TEXT_INPUT_FLAG_AUTOCOMPLETE_ON) {
    hint |= ZWP_TEXT_INPUT_V1_CONTENT_HINT_AUTO_COMPLETION;
  }
  if (input_flags & TEXT_INPUT_FLAG_AUTOCORRECT_ON) {
    hint |= ZWP_TEXT_INPUT_V1_CONTENT_HINT_AUTO_CORRECTION;
  }
  // No good match. Fallback to AUTO_CORRECTION.
  if (input_flags & TEXT_INPUT_FLAG_SPELLCHECK_ON) {
    hint |= ZWP_TEXT_INPUT_V1_CONTENT_HINT_AUTO_CORRECTION;
  }
  if (input_flags & TEXT_INPUT_FLAG_AUTOCAPITALIZE_CHARACTERS) {
    hint |= ZWP_TEXT_INPUT_V1_CONTENT_HINT_UPPERCASE;
  }
  if (input_flags & TEXT_INPUT_FLAG_AUTOCAPITALIZE_WORDS) {
    hint |= ZWP_TEXT_INPUT_V1_CONTENT_HINT_TITLECASE;
  }
  if (input_flags & TEXT_INPUT_FLAG_AUTOCAPITALIZE_SENTENCES) {
    hint |= ZWP_TEXT_INPUT_V1_CONTENT_HINT_AUTO_CAPITALIZATION;
  }
  if (input_flags & TEXT_INPUT_FLAG_HAS_BEEN_PASSWORD) {
    hint |= ZWP_TEXT_INPUT_V1_CONTENT_HINT_PASSWORD;
  }
  return hint;
}

// Parses the content of |array|, and creates a map of modifiers.
// The content of array is just a concat of modifier names in c-style string
// (i.e., '\0' terminated string), thus this splits the whole byte array by
// '\0' character.
std::vector<std::string> ParseModifiersMap(wl_array* array) {
  return base::SplitString(
      std::string_view(static_cast<char*>(array->data),
                       array->size - 1),  // exclude trailing '\0'.
      std::string_view("\0", 1),          // '\0' as a delimiter.
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
}

// Returns ImeTextSpan style to be assigned. Maybe nullopt if it is not
// supported.
std::optional<SpanStyle::Style> ConvertStyle(uint32_t style) {
  switch (style) {
    case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_DEFAULT:
      return {{ImeTextSpan::Type::kComposition, ImeTextSpan::Thickness::kNone}};
    case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_HIGHLIGHT:
      return {
          {ImeTextSpan::Type::kComposition, ImeTextSpan::Thickness::kThick}};
    case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_UNDERLINE:
      return {{ImeTextSpan::Type::kComposition, ImeTextSpan::Thickness::kThin}};
    case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_SELECTION:
      return {{ImeTextSpan::Type::kSuggestion, ImeTextSpan::Thickness::kNone}};
    case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_INCORRECT:
      return {{ImeTextSpan::Type::kMisspellingSuggestion,
               ImeTextSpan::Thickness::kNone}};
    case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_NONE:
    case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_ACTIVE:
    case ZWP_TEXT_INPUT_V1_PREEDIT_STYLE_INACTIVE:
    default:
      VLOG(1) << "Unsupported style. Skipped: " << style;
  }
  return std::nullopt;
}

}  // namespace

ZwpTextInputV1Impl::ZwpTextInputV1Impl(
    WaylandConnection* connection,
    zwp_text_input_manager_v1* text_input_manager)
    : connection_(connection) {
  static constexpr zwp_text_input_v1_listener kTextInputListener = {
      .enter = &OnEnter,
      .leave = &OnLeave,
      .modifiers_map = &OnModifiersMap,
      .input_panel_state = &OnInputPanelState,
      .preedit_string = &OnPreeditString,
      .preedit_styling = &OnPreeditStyling,
      .preedit_cursor = &OnPreeditCursor,
      .commit_string = &OnCommitString,
      .cursor_position = &OnCursorPosition,
      .delete_surrounding_text = &OnDeleteSurroundingText,
      .keysym = &OnKeysym,
      .language = &OnLanguage,
      .text_direction = &OnTextDirection,
  };

  obj_ = wl::Object<zwp_text_input_v1>(
      zwp_text_input_manager_v1_create_text_input(text_input_manager));
  DCHECK(obj_.get());
  zwp_text_input_v1_add_listener(obj_.get(), &kTextInputListener, this);
}

ZwpTextInputV1Impl::~ZwpTextInputV1Impl() = default;

void ZwpTextInputV1Impl::Reset() {
  ResetInputEventState();
  zwp_text_input_v1_reset(obj_.get());
}

void ZwpTextInputV1Impl::Activate(WaylandWindow* window) {
  DCHECK(connection_->seat());
  zwp_text_input_v1_activate(obj_.get(), connection_->seat()->wl_object(),
                             window->root_surface()->surface());
}

void ZwpTextInputV1Impl::SetClient(ZwpTextInputV1Client* context) {
  client_ = context;
}

void ZwpTextInputV1Impl::OnClientDestroyed(ZwpTextInputV1Client* context) {
  if (client_ == context) {
    client_ = nullptr;
    Deactivate();
  }
}

void ZwpTextInputV1Impl::Deactivate() {
  DCHECK(connection_->seat());

  zwp_text_input_v1_deactivate(obj_.get(), connection_->seat()->wl_object());
}

void ZwpTextInputV1Impl::ShowInputPanel() {
  zwp_text_input_v1_show_input_panel(obj_.get());
}

void ZwpTextInputV1Impl::HideInputPanel() {
  zwp_text_input_v1_hide_input_panel(obj_.get());
}

void ZwpTextInputV1Impl::SetCursorRect(const gfx::Rect& rect) {
  zwp_text_input_v1_set_cursor_rectangle(obj_.get(), rect.x(), rect.y(),
                                         rect.width(), rect.height());
}

void ZwpTextInputV1Impl::SetSurroundingText(const std::string& text,
                                            const gfx::Range& preedit_range,
                                            const gfx::Range& selection_range) {
  zwp_text_input_v1_set_surrounding_text(
      obj_.get(), text.c_str(), selection_range.start(), selection_range.end());
}

void ZwpTextInputV1Impl::SetContentType(ui::TextInputType type,
                                        uint32_t flags,
                                        bool should_do_learning) {
  uint32_t content_purpose = InputTypeToContentPurpose(type);
  uint32_t content_hint = InputFlagsToContentHint(flags);
  if (!should_do_learning) {
    content_hint |= ZWP_TEXT_INPUT_V1_CONTENT_HINT_SENSITIVE_DATA;
  }
  zwp_text_input_v1_set_content_type(obj_.get(), content_hint, content_purpose);
}

void ZwpTextInputV1Impl::ResetInputEventState() {
  spans_.clear();
  preedit_cursor_ = -1;
}

// static
void ZwpTextInputV1Impl::OnEnter(void* data,
                                 struct zwp_text_input_v1* text_input,
                                 struct wl_surface* surface) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void ZwpTextInputV1Impl::OnLeave(void* data,
                                 struct zwp_text_input_v1* text_input) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void ZwpTextInputV1Impl::OnModifiersMap(void* data,
                                        struct zwp_text_input_v1* text_input,
                                        struct wl_array* map) {
  auto* self = static_cast<ZwpTextInputV1Impl*>(data);
  self->client_->OnModifiersMap(ParseModifiersMap(map));
}

// static
void ZwpTextInputV1Impl::OnInputPanelState(void* data,
                                           struct zwp_text_input_v1* text_input,
                                           uint32_t state) {
  auto* self = static_cast<ZwpTextInputV1Impl*>(data);
  self->client_->OnInputPanelState(state);
}

// static
void ZwpTextInputV1Impl::OnPreeditString(void* data,
                                         struct zwp_text_input_v1* text_input,
                                         uint32_t serial,
                                         const char* text,
                                         const char* commit) {
  auto* self = static_cast<ZwpTextInputV1Impl*>(data);
  auto spans = std::move(self->spans_);
  int32_t preedit_cursor = self->preedit_cursor_;
  self->ResetInputEventState();
  self->client_->OnPreeditString(text, spans,
                                 preedit_cursor < 0
                                     ? gfx::Range::InvalidRange()
                                     : gfx::Range(preedit_cursor));
}

// static
void ZwpTextInputV1Impl::OnPreeditStyling(void* data,
                                          struct zwp_text_input_v1* text_input,
                                          uint32_t index,
                                          uint32_t length,
                                          uint32_t style) {
  auto* self = static_cast<ZwpTextInputV1Impl*>(data);
  self->spans_.push_back(SpanStyle{index, length, ConvertStyle(style)});
}

// static
void ZwpTextInputV1Impl::OnPreeditCursor(void* data,
                                         struct zwp_text_input_v1* text_input,
                                         int32_t index) {
  auto* self = static_cast<ZwpTextInputV1Impl*>(data);
  self->preedit_cursor_ = index;
}

// static
void ZwpTextInputV1Impl::OnCommitString(void* data,
                                        struct zwp_text_input_v1* text_input,
                                        uint32_t serial,
                                        const char* text) {
  auto* self = static_cast<ZwpTextInputV1Impl*>(data);
  self->ResetInputEventState();
  self->client_->OnCommitString(text);
}

// static
void ZwpTextInputV1Impl::OnCursorPosition(void* data,
                                          struct zwp_text_input_v1* text_input,
                                          int32_t index,
                                          int32_t anchor) {
  auto* self = static_cast<ZwpTextInputV1Impl*>(data);
  self->client_->OnCursorPosition(index, anchor);
}

// static
void ZwpTextInputV1Impl::OnDeleteSurroundingText(
    void* data,
    struct zwp_text_input_v1* text_input,
    int32_t index,
    uint32_t length) {
  auto* self = static_cast<ZwpTextInputV1Impl*>(data);
  self->client_->OnDeleteSurroundingText(index, length);
}

// static
void ZwpTextInputV1Impl::OnKeysym(void* data,
                                  struct zwp_text_input_v1* text_input,
                                  uint32_t serial,
                                  uint32_t time,
                                  uint32_t key,
                                  uint32_t state,
                                  uint32_t modifiers) {
  auto* self = static_cast<ZwpTextInputV1Impl*>(data);
  self->client_->OnKeysym(key, state, modifiers, time);
}

// static
void ZwpTextInputV1Impl::OnLanguage(void* data,
                                    struct zwp_text_input_v1* text_input,
                                    uint32_t serial,
                                    const char* language) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void ZwpTextInputV1Impl::OnTextDirection(void* data,
                                         struct zwp_text_input_v1* text_input,
                                         uint32_t serial,
                                         uint32_t direction) {
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace ui
