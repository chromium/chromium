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
  zwp_text_input_v3_commit(obj_.get());
  zwp_text_input_v3_enable(obj_.get());
  zwp_text_input_v3_commit(obj_.get());
}

void ZWPTextInputWrapperV3::Activate(WaylandWindow* window,
                                     TextInputClient::FocusReason reason) {
  zwp_text_input_v3_enable(obj_.get());
  zwp_text_input_v3_commit(obj_.get());
}

void ZWPTextInputWrapperV3::Deactivate() {
  zwp_text_input_v3_disable(obj_.get());
  zwp_text_input_v3_commit(obj_.get());
}

void ZWPTextInputWrapperV3::ShowInputPanel() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ZWPTextInputWrapperV3::HideInputPanel() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ZWPTextInputWrapperV3::SetCursorRect(const gfx::Rect& rect) {
  NOTIMPLEMENTED_LOG_ONCE();
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
  NOTIMPLEMENTED_LOG_ONCE();
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
  NOTIMPLEMENTED_LOG_ONCE();
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
