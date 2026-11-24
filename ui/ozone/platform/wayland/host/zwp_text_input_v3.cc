// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/zwp_text_input_v3.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notimplemented.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "ui/base/wayland/wayland_client_input_types.h"
#include "ui/gfx/range/range.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/span_style.h"
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

ZwpTextInputV3Impl::ImeData::ImeData() = default;
ZwpTextInputV3Impl::ImeData::~ImeData() = default;
void ZwpTextInputV3Impl::ImeData::Reset() {
  surrounding_text.reset();
  cursor_rect.reset();
  content_type.reset();
}

ZwpTextInputV3Impl::InputEvents::InputEvents()
    : preedit(std::make_unique<PreeditData>()) {}
ZwpTextInputV3Impl::InputEvents::~InputEvents() = default;
void ZwpTextInputV3Impl::InputEvents::Reset() {
  preedit = std::make_unique<PreeditData>();
  commit = {};
  delete_surrounding_text.reset();
}

ZwpTextInputV3Impl::ZwpTextInputV3Impl(
    WaylandConnection* connection,
    zwp_text_input_manager_v3* text_input_manager)
    : connection_(connection) {
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

ZwpTextInputV3Impl::~ZwpTextInputV3Impl() = default;

void ZwpTextInputV3Impl::Reset() {
  // There is no explicit reset API in v3. See [1].
  //
  // If the input method has a preedit, Chrome has just committed or canceled
  // the composition (e.g. by a mouse click in the text field), but the input
  // method keeps composing and would later commit the preedit text again at
  // the new caret position. Disable and enable text input to make the input
  // method drop its composition, as it does when another text field gets
  // focus. Anything the input method commits in response usually arrives
  // shortly after the reset, while InputMethodAuraLinux still ignores commits.
  //
  // The enable is posted, so that a focus change right after the reset (see
  // InputMethodAuraLinux::OnWillChangeFocusedClient()) disables and enables
  // only once, and with the new client's state.
  //
  // Disabling and enabling on every reset made the input method grab the
  // keyboard unexpectedly on mutter and kwin (crbug.com/412266383), e.g. on
  // tab switches, so only do it when there is a preedit to drop.
  //
  // [1]
  // https://gitlab.freedesktop.org/wayland/wayland-protocols/-/merge_requests/34
  const bool has_preedit =
      enabled_content_type_ && !applied_input_events_.preedit->text.empty();

  // Clear last sent values so that the next ones are sent even if unchanged.
  sent_ime_data_.Reset();
  // Even though chromium expects only preedit to be reset, the surrounding text
  // in fact could change along with reset being called if composition was
  // canceled internally. So we shouldn't keep old surrounding text anyway. See
  // related crbug.com/353915732 where surrounding text update is not sent after
  // reset when composition is canceled.
  pending_input_events_.Reset();
  applied_input_events_.Reset();

  if (has_preedit) {
    // The cached surrounding text still has the caret at the start of the
    // dropped preedit, without the text that was just committed. Don't resend
    // it on enable; the client sends the updated text afterwards.
    latest_surrounding_text_.reset();
    zwp_text_input_v3_disable(obj_.get());
    Commit();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ZwpTextInputV3Impl::SendEnable,
                                  enable_after_reset_factory_.GetWeakPtr()));
  }
}

void ZwpTextInputV3Impl::ShowInputPanel() {
  if (!enabled_content_type_) {
    return;
  }
  if (enable_after_reset_factory_.HasWeakPtrs()) {
    SendEnable();
  }
  // text-input-v3 version 1 has no request to show the input panel.
  // Compositors such as mutter show it when an already enabled text input is
  // enabled again, which is also what GTK does on a click or tap. The input
  // method keeps its composition, so keep the received input events for
  // Reset().
  SendEnableRequest();
}

void ZwpTextInputV3Impl::SetClient(ZwpTextInputV3Client* context) {
  client_ = context;
}

void ZwpTextInputV3Impl::OnClientDestroyed(ZwpTextInputV3Client* context) {
  if (client_ == context) {
    client_ = nullptr;
    ClearCachedState();
    Disable();
  }
}

void ZwpTextInputV3Impl::Enable(ui::TextInputType type,
                                uint32_t flags,
                                bool should_do_learning) {
  enabled_content_type_ = GetContentType(type, flags, should_do_learning);
  SendEnable();
}

void ZwpTextInputV3Impl::SendEnable() {
  enable_after_reset_factory_.InvalidateWeakPtrs();
  // Pending state is reset on enable.
  pending_input_events_.Reset();
  applied_input_events_.Reset();
  SendEnableRequest();
}

void ZwpTextInputV3Impl::SendEnableRequest() {
  sent_ime_data_.Reset();

  zwp_text_input_v3_enable(obj_.get());
  SendContentType(*enabled_content_type_);

  // Per the protocol, all state is reset by the compositor on each committed
  // enable request, so any previously sent state is no longer valid. Resend the
  // most recent surrounding text and cursor rectangle in the same commit, as
  // they may not otherwise be requested again (e.g. when the window regains
  // text input focus and the text is unchanged).
  if (latest_surrounding_text_) {
    SendSurroundingText(*latest_surrounding_text_);
  }
  if (latest_cursor_rect_) {
    SendCursorRect(*latest_cursor_rect_);
  }
  Commit();
}

void ZwpTextInputV3Impl::ClearCachedState() {
  latest_surrounding_text_.reset();
  latest_cursor_rect_.reset();
}

void ZwpTextInputV3Impl::Disable() {
  enabled_content_type_.reset();
  // Do not process pending input events after deactivating.
  pending_input_events_.Reset();
  if (enable_after_reset_factory_.HasWeakPtrs()) {
    // Reset() has already disabled text input; just drop its pending enable.
    enable_after_reset_factory_.InvalidateWeakPtrs();
    return;
  }
  zwp_text_input_v3_disable(obj_.get());
  Commit();
}

ZwpTextInputV3Impl::ContentType ZwpTextInputV3Impl::GetContentType(
    ui::TextInputType type,
    uint32_t flags,
    bool should_do_learning) {
  uint32_t content_hint = InputFlagsToContentHint(flags);
  if (!should_do_learning) {
    content_hint |= ZWP_TEXT_INPUT_V3_CONTENT_HINT_SENSITIVE_DATA;
  }
  uint32_t content_purpose = InputTypeToContentPurpose(type);
  return ContentType(content_hint, content_purpose);
}

void ZwpTextInputV3Impl::SetCursorRect(const gfx::Rect& rect) {
  latest_cursor_rect_ = rect;
  if (sent_ime_data_.cursor_rect && *sent_ime_data_.cursor_rect == rect) {
    // This is to avoid a loop in sending cursor rect and receiving pre-edit
    // string.
    return;
  }
  SendCursorRect(rect);
  Commit();
}

void ZwpTextInputV3Impl::SendCursorRect(const gfx::Rect& rect) {
  zwp_text_input_v3_set_cursor_rectangle(obj_.get(), rect.x(), rect.y(),
                                         rect.width(), rect.height());
  sent_ime_data_.cursor_rect = std::make_unique<gfx::Rect>(rect);
}

void ZwpTextInputV3Impl::SetSurroundingText(
    const std::string& text_with_preedit,
    const gfx::Range& preedit_range,
    const gfx::Range& selection_range) {
  auto text = text_with_preedit;
  int32_t anchor, cursor;
  if (!preedit_range.is_empty()) {
    CHECK(preedit_range.IsBoundedBy({0, text_with_preedit.length()}));
    const size_t preedit_min = preedit_range.GetMin();
    const size_t preedit_max = preedit_range.GetMax();
    // Remove preedit portion from surrounding text
    text.erase(preedit_min, preedit_range.length());
    // Now re-calculate selection range
    if (selection_range.IsValid()) {
      auto selection_start = selection_range.start();
      auto selection_end = selection_range.end();
      anchor = selection_start -
               (selection_start <= preedit_min
                    ? 0
                    : std::min(selection_start, preedit_max) - preedit_min);
      cursor = selection_end -
               (selection_end <= preedit_min
                    ? 0
                    : std::min(selection_end, preedit_max) - preedit_min);

    } else {
      // Invalid selection range. Put cursor at preedit position.
      anchor = preedit_min;
      cursor = preedit_min;
    }
  } else {
    anchor = base::checked_cast<int32_t>(
        selection_range.IsValid() ? selection_range.start() : text.length());
    cursor = base::checked_cast<int32_t>(
        selection_range.IsValid() ? selection_range.end() : text.length());
  }
  latest_surrounding_text_ = SurroundingText(std::move(text), cursor, anchor);
  if (sent_ime_data_.surrounding_text &&
      *sent_ime_data_.surrounding_text == *latest_surrounding_text_) {
    return;
  }
  SendSurroundingText(*latest_surrounding_text_);
  Commit();
}

void ZwpTextInputV3Impl::SendSurroundingText(
    const SurroundingText& surrounding_text) {
  zwp_text_input_v3_set_surrounding_text(
      obj_.get(), surrounding_text.text.c_str(), surrounding_text.cursor,
      surrounding_text.anchor);
  sent_ime_data_.surrounding_text =
      std::make_unique<SurroundingText>(surrounding_text);
}

void ZwpTextInputV3Impl::SetContentType(ui::TextInputType type,
                                        uint32_t flags,
                                        bool should_do_learning) {
  const ContentType content_type =
      GetContentType(type, flags, should_do_learning);
  if (enabled_content_type_) {
    enabled_content_type_ = content_type;
  }
  if (sent_ime_data_.content_type &&
      *sent_ime_data_.content_type == content_type) {
    return;
  }
  SendContentType(content_type);
  Commit();
}

void ZwpTextInputV3Impl::SendContentType(const ContentType& content_type) {
  zwp_text_input_v3_set_content_type(obj_.get(), content_type.content_hint,
                                     content_type.content_purpose);
  sent_ime_data_.content_type = std::make_unique<ContentType>(content_type);
}

void ZwpTextInputV3Impl::Commit() {
  zwp_text_input_v3_commit(obj_.get());
}

void ZwpTextInputV3Impl::OnEnter(void* data,
                                 struct zwp_text_input_v3* text_input,
                                 struct wl_surface* surface) {
  auto* self = static_cast<ZwpTextInputV3Impl*>(data);
  if (auto* window = wl::RootWindowFromWlSurface(surface)) {
    self->connection_->window_manager()->SetTextInputFocusedWindow(window);
  }
}

void ZwpTextInputV3Impl::OnLeave(void* data,
                                 struct zwp_text_input_v3* text_input,
                                 struct wl_surface* surface) {
  auto* self = static_cast<ZwpTextInputV3Impl*>(data);
  self->connection_->window_manager()->SetTextInputFocusedWindow(nullptr);
}

void ZwpTextInputV3Impl::OnPreeditString(void* data,
                                         struct zwp_text_input_v3* text_input,
                                         const char* text,
                                         int32_t cursor_begin,
                                         int32_t cursor_end) {
  auto* self = static_cast<ZwpTextInputV3Impl*>(data);
  self->pending_input_events_.preedit = std::make_unique<PreeditData>(
      text ? text : std::string(), cursor_begin, cursor_end);
}

void ZwpTextInputV3Impl::OnCommitString(void* data,
                                        struct zwp_text_input_v3* text_input,
                                        const char* text) {
  auto* self = static_cast<ZwpTextInputV3Impl*>(data);
  self->pending_input_events_.commit = text ? text : std::string();
}

void ZwpTextInputV3Impl::OnDeleteSurroundingText(
    void* data,
    struct zwp_text_input_v3* text_input,
    uint32_t before_length,
    uint32_t after_length) {
  auto* self = static_cast<ZwpTextInputV3Impl*>(data);
  self->pending_input_events_.delete_surrounding_text =
      std::make_unique<DeleteSurroundingText>(before_length, after_length);
}

void ZwpTextInputV3Impl::OnDone(void* data,
                                struct zwp_text_input_v3* text_input,
                                uint32_t serial) {
  // The serial is not used, see `sent_ime_data_`.
  auto* self = static_cast<ZwpTextInputV3Impl*>(data);

  if (!self->client_) {
    return;
  }
  const auto& commit_string = self->pending_input_events_.commit;
  const auto& preedit_data = self->pending_input_events_.preedit;
  CHECK(preedit_data);
  bool events_applied = false;
  if (const auto& delete_surrounding_text =
          self->pending_input_events_.delete_surrounding_text) {
    // The deletion is relative to the current cursor of the client, which
    // tracks the text state including the input events applied since the
    // surrounding text was last sent.
    self->client_->OnDeleteSurroundingText(
        delete_surrounding_text->before_length,
        delete_surrounding_text->after_length);
    self->applied_input_events_.delete_surrounding_text =
        std::move(self->pending_input_events_.delete_surrounding_text);
    events_applied = true;
  }

  if (!commit_string.empty()) {
    // Replace the existing preedit with the commit string.
    self->client_->OnCommitString(commit_string);
    self->applied_input_events_.commit =
        std::move(self->pending_input_events_.commit);
    events_applied = true;
  }

  // Finally process any new preedit string (only if it changed).
  gfx::Range preedit_cursor =
      (preedit_data->cursor_begin < 0 || preedit_data->cursor_end < 0)
          ? gfx::Range::InvalidRange()
          : gfx::Range(preedit_data->cursor_begin, preedit_data->cursor_end);
  CHECK(self->applied_input_events_.preedit);
  if (*self->applied_input_events_.preedit !=
      *self->pending_input_events_.preedit) {
    self->client_->OnPreeditString(preedit_data->text, {}, preedit_cursor);
    self->applied_input_events_.preedit =
        std::move(self->pending_input_events_.preedit);
    events_applied = true;
  }

  self->pending_input_events_.Reset();

  // The input method expects the surrounding text sent in response to done to
  // reflect the applied events. Only do this if the client supports
  // surrounding text, i.e. it has been sent since the text input was enabled.
  if (events_applied && self->sent_ime_data_.surrounding_text) {
    self->client_->OnInputEventsApplied();
  }
}

}  // namespace ui
