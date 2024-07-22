// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/zwp_text_input_wrapper_v3.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notimplemented.h"
#include "base/numerics/safe_conversions.h"
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
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL;
    case TEXT_INPUT_TYPE_TEXT:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL;
    case TEXT_INPUT_TYPE_PASSWORD:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PASSWORD;
    case TEXT_INPUT_TYPE_SEARCH:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL;
    case TEXT_INPUT_TYPE_EMAIL:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_EMAIL;
    case TEXT_INPUT_TYPE_NUMBER:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NUMBER;
    case TEXT_INPUT_TYPE_TELEPHONE:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PHONE;
    case TEXT_INPUT_TYPE_URL:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_URL;
    case TEXT_INPUT_TYPE_DATE:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_DATE;
    case TEXT_INPUT_TYPE_DATE_TIME:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_DATETIME;
    case TEXT_INPUT_TYPE_DATE_TIME_LOCAL:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_DATETIME;
    case TEXT_INPUT_TYPE_MONTH:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_DATE;
    case TEXT_INPUT_TYPE_TIME:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_TIME;
    case TEXT_INPUT_TYPE_WEEK:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_DATE;
    case TEXT_INPUT_TYPE_TEXT_AREA:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL;
    case TEXT_INPUT_TYPE_CONTENT_EDITABLE:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL;
    case TEXT_INPUT_TYPE_DATE_TIME_FIELD:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_DATETIME;
    case TEXT_INPUT_TYPE_NULL:
      return ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL;
  }
}

// Converts Chrome's TextInputType into wayland's content_hint.
uint32_t InputFlagsToContentHint(int input_flags) {
  uint32_t hint = 0;
  if (input_flags & TEXT_INPUT_FLAG_AUTOCOMPLETE_ON) {
    hint |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_COMPLETION;
  }
  if (input_flags & TEXT_INPUT_FLAG_SPELLCHECK_ON) {
    hint |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_SPELLCHECK;
  }
  // No good match. Fallback to SPELLCHECK.
  if (input_flags & TEXT_INPUT_FLAG_AUTOCORRECT_ON) {
    hint |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_SPELLCHECK;
  }
  if (input_flags & TEXT_INPUT_FLAG_AUTOCAPITALIZE_CHARACTERS) {
    hint |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_AUTO_CAPITALIZATION;
  }
  if (input_flags & TEXT_INPUT_FLAG_AUTOCAPITALIZE_WORDS) {
    hint |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_AUTO_CAPITALIZATION;
  }
  if (input_flags & TEXT_INPUT_FLAG_AUTOCAPITALIZE_SENTENCES) {
    hint |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_AUTO_CAPITALIZATION;
  }
  if (input_flags & TEXT_INPUT_FLAG_HAS_BEEN_PASSWORD) {
    hint |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_HIDDEN_TEXT;
    hint |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_SENSITIVE_DATA;
  }
  return hint;
}

}  // namespace

ZWPTextInputWrapperV3::ZWPTextInputWrapperV3(
    WaylandConnection* connection,
    ZWPTextInputWrapperClient* client,
    zwp_text_input_manager_v3* text_input_manager)
    : connection_(connection), client_(client) {
  static constexpr zwp_text_input_v3_listener kTextInputListener = {
      &OnEnter,
      &OnLeave,
      &OnPreeditString,
      &OnCommitString,
      &OnDeleteSurroundingText,
      &OnDone,
  };

  CHECK(text_input_manager);
  auto* text_input = zwp_text_input_manager_v3_get_text_input(
      text_input_manager, connection_->seat()->wl_object());
  obj_ = wl::Object<zwp_text_input_v3>(text_input);

  zwp_text_input_v3_add_listener(text_input, &kTextInputListener, this);
}

ZWPTextInputWrapperV3::~ZWPTextInputWrapperV3() = default;

void ZWPTextInputWrapperV3::Reset() {
  // Clear last sent values.
  ResetLastSentValues();
  // There is no explicit reset API in v3. See [1].
  // So use disable+enable to force a reset.
  //
  // TODO(crbug.com/352352898) Calling enable below as per text-input-v3 will
  // reset all state including surrounding text but chromium expects reset to
  // only clear preedit, see WaylandInputMethodContext::Reset(). This needs to
  // be addressed on the protocol side and/or chromium side so that they match.
  // If no reset is implemented at all, it can lead to bad user experience,
  // e.g. preedit being duplicated if composition is aborted on the chromium
  // side by clicking in the input field. So the logic below is still needed
  // until a proper fix is in place.
  //
  // [1]
  // https://gitlab.freedesktop.org/wayland/wayland-protocols/-/merge_requests/34
  zwp_text_input_v3_disable(obj_.get());
  Commit();
  // Pending state should be reset on enable as per the protocol. Even though
  // chromium expects only preedit to be reset, the surrounding text in fact
  // could change along with reset being called if composition was canceled
  // internally. So we shouldn't keep old surrounding text anyway. See related
  // crbug.com/353915732 where surrounding text update is not sent after reset
  // when composition is canceled.
  ResetPendingSetRequests();
  zwp_text_input_v3_enable(obj_.get());
  Commit();
}

void ZWPTextInputWrapperV3::Activate(WaylandWindow* window,
                                     TextInputClient::FocusReason reason) {
  // Pending state is reset on enable.
  ResetPendingSetRequests();
  zwp_text_input_v3_enable(obj_.get());
  Commit();
}

void ZWPTextInputWrapperV3::Deactivate() {
  // Avoid sending pending requests if done is received after disabling.
  ResetPendingSetRequests();
  zwp_text_input_v3_disable(obj_.get());
  Commit();
}

void ZWPTextInputWrapperV3::ShowInputPanel() {
  // Not directly supported in zwp_text_input_v3
  // Enable again to show the screen keyboard in GNOME:
  // https://gitlab.gnome.org/GNOME/mutter/-/merge_requests/1543#note_1051704
  // We do not reset the pending requests here because this may be called after
  // sending a request like surrounding text before done event is received, in
  // which case the pending surrounding text should still be sent.
  zwp_text_input_v3_enable(obj_.get());
  Commit();
}

void ZWPTextInputWrapperV3::HideInputPanel() {
  // Unsupported in zwp_text_input_v3 yet. To be supported soon as per wayland
  // governance meeting on 2024-07-02:
  // https://gitlab.freedesktop.org/wayland/wayland-protocols/-/wikis/meetings
  //
  // Some earlier notes in
  // https://lists.freedesktop.org/archives/wayland-devel/2018-March/037341.html
  NOTIMPLEMENTED_LOG_ONCE();
}

void ZWPTextInputWrapperV3::SetCursorRect(const gfx::Rect& rect) {
  if (last_sent_cursor_rect_ == rect) {
    // This is to avoid a loop in sending cursor rect and receiving pre-edit
    // string.
    return;
  }
  if (commit_count_ != last_done_serial_) {
    pending_set_cursor_rect_ = rect;
    return;
  }
  SendCursorRect(rect);
  Commit();
}

void ZWPTextInputWrapperV3::SendCursorRect(const gfx::Rect& rect) {
  CHECK_EQ(commit_count_, last_done_serial_);
  zwp_text_input_v3_set_cursor_rectangle(obj_.get(), rect.x(), rect.y(),
                                         rect.width(), rect.height());
  last_sent_cursor_rect_ = rect;
}

void ZWPTextInputWrapperV3::SetSurroundingText(
    const std::string& text,
    const gfx::Range& preedit_range,
    const gfx::Range& selection_range) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ZWPTextInputWrapperV3::SetContentType(ui::TextInputType type,
                                           ui::TextInputMode mode,
                                           uint32_t flags,
                                           bool should_do_learning,
                                           bool can_compose_inline) {
  // V3 is not used with chromium text-input extension protocol. So mode,
  // should_do_learning and can_compose_inline are not used.
  uint32_t content_hint = InputFlagsToContentHint(flags);
  uint32_t content_purpose = InputTypeToContentPurpose(type);
  ContentType content_type{content_hint, content_purpose};
  if (last_sent_content_type_ == content_type) {
    return;
  }
  if (commit_count_ != last_done_serial_) {
    pending_set_content_type_ = content_type;
    return;
  }
  SendContentType(content_type);
  Commit();
}

void ZWPTextInputWrapperV3::SendContentType(const ContentType& content_type) {
  CHECK_EQ(commit_count_, last_done_serial_);
  zwp_text_input_v3_set_content_type(obj_.get(), content_type.content_hint,
                                     content_type.content_purpose);
  last_sent_content_type_ = content_type;
}

void ZWPTextInputWrapperV3::ApplyPendingSetRequests() {
  bool commit = false;
  if (auto content_type = pending_set_content_type_) {
    SendContentType(*content_type);
    commit = true;
  }
  if (auto cursor_rect = pending_set_cursor_rect_) {
    SendCursorRect(*cursor_rect);
    commit = true;
  }
  // clear pending set requests
  ResetPendingSetRequests();
  if (commit) {
    Commit();
  }
}

void ZWPTextInputWrapperV3::ResetPendingSetRequests() {
  pending_set_cursor_rect_.reset();
  pending_set_content_type_.reset();
}

void ZWPTextInputWrapperV3::ResetLastSentValues() {
  last_sent_cursor_rect_ = {};
  last_sent_content_type_ = {};
}

void ZWPTextInputWrapperV3::Commit() {
  zwp_text_input_v3_commit(obj_.get());
  // It will wrap around to 0 once it reaches uint32_t max value. It is
  // expected that this will occur on the compositor side as well.
  ++commit_count_;
}

void ZWPTextInputWrapperV3::OnEnter(void* data,
                                    struct zwp_text_input_v3* text_input,
                                    struct wl_surface* surface) {
  // Same as text-input-v1, we don't use this for text-input focus changes and
  // instead use wayland keyboard enter/leave events to activate or deactivate
  // text-input.
  NOTIMPLEMENTED_LOG_ONCE();
}

void ZWPTextInputWrapperV3::OnLeave(void* data,
                                    struct zwp_text_input_v3* text_input,
                                    struct wl_surface* surface) {
  // Same as text-input-v1, we don't use this for text-input focus changes and
  // instead use wayland keyboard enter/leave events to activate or deactivate
  // text-input.
  NOTIMPLEMENTED_LOG_ONCE();
}

void ZWPTextInputWrapperV3::OnPreeditString(
    void* data,
    struct zwp_text_input_v3* text_input,
    const char* text,
    int32_t cursor_begin,
    int32_t cursor_end) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ZWPTextInputWrapperV3::OnCommitString(void* data,
                                           struct zwp_text_input_v3* text_input,
                                           const char* text) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ZWPTextInputWrapperV3::OnDeleteSurroundingText(
    void* data,
    struct zwp_text_input_v3* text_input,
    uint32_t before_length,
    uint32_t after_length) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ZWPTextInputWrapperV3::OnDone(void* data,
                                   struct zwp_text_input_v3* text_input,
                                   uint32_t serial) {
  // TODO(crbug.com/40113488) apply preedit, commit, delete surrounding
  auto* self = static_cast<ZWPTextInputWrapperV3*>(data);
  self->last_done_serial_ = serial;
  if (self->last_done_serial_ == self->commit_count_) {
    self->ApplyPendingSetRequests();
  }
}

// The following methods are not applicable to text-input-v3 because they are
// needed in Exo with text-input-v1 protocol + extended text input protocol.

bool ZWPTextInputWrapperV3::HasAdvancedSurroundingTextSupport() const {
  return false;
}

void ZWPTextInputWrapperV3::SetSurroundingTextOffsetUtf16(
    uint32_t offset_utf16) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ZWPTextInputWrapperV3::SetGrammarFragmentAtCursor(
    const ui::GrammarFragment& fragment) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ZWPTextInputWrapperV3::SetAutocorrectInfo(
    const gfx::Range& autocorrect_range,
    const gfx::Rect& autocorrect_bounds) {
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace ui
