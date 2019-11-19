// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_input_method_context.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/ime_text_span.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_code_conversion_xkb.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/gfx/range/range.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/zwp_text_input_wrapper_v1.h"
#include "ui/ozone/public/ozone_switches.h"

namespace ui {

namespace {

constexpr int kXkbKeycodeOffset = 8;

}  // namespace

WaylandInputMethodContext::WaylandInputMethodContext(
    WaylandConnection* connection,
    LinuxInputMethodContextDelegate* delegate,
    bool is_simple,
    const EventDispatchCallback& callback)
    : connection_(connection),
      delegate_(delegate),
      is_simple_(is_simple),
      callback_(callback),
      text_input_(nullptr) {
  Init();
}

WaylandInputMethodContext::~WaylandInputMethodContext() {
  if (text_input_) {
    text_input_->Deactivate();
    text_input_->HideInputPanel();
  }
}

void WaylandInputMethodContext::Init(bool initialize_for_testing) {
  bool use_ozone_wayland_vkb =
      initialize_for_testing ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWaylandIme);

  // If text input instance is not created then all ime context operations
  // are noop. This option is because in some environments someone might not
  // want to enable ime/virtual keyboard even if it's available.
  if (use_ozone_wayland_vkb && !is_simple_ && !text_input_ &&
      connection_->text_input_manager_v1()) {
    text_input_ = std::make_unique<ZWPTextInputWrapperV1>(
        connection_->text_input_manager_v1());
    text_input_->Initialize(connection_, this);
  }
}

bool WaylandInputMethodContext::DispatchKeyEvent(
    const ui::KeyEvent& key_event) {
  if (key_event.type() != ET_KEY_PRESSED ||
      !character_composer_.FilterKeyPress(key_event))
    return false;

  // CharacterComposer consumed the key event. Update the composition text.
  UpdatePreeditText(character_composer_.preedit_string());
  auto composed = character_composer_.composed_character();
  if (!composed.empty())
    delegate_->OnCommit(composed);
  return true;
}

void WaylandInputMethodContext::UpdatePreeditText(
    const base::string16& preedit_text) {
  CompositionText preedit;
  preedit.text = preedit_text;
  auto length = preedit.text.size();

  preedit.selection = gfx::Range(length);
  preedit.ime_text_spans.push_back(
      ImeTextSpan(ImeTextSpan::Type::kComposition, 0, length,
                  ImeTextSpan::Thickness::kThin, SK_ColorTRANSPARENT));
  delegate_->OnPreeditChanged(preedit);
}

void WaylandInputMethodContext::Reset() {
  character_composer_.Reset();
  if (text_input_)
    text_input_->Reset();
}

void WaylandInputMethodContext::Focus() {
  WaylandWindow* window =
      connection_->wayland_window_manager()->GetCurrentKeyboardFocusedWindow();
  if (!text_input_ || !window)
    return;

  text_input_->Activate(window);
  text_input_->ShowInputPanel();
}

void WaylandInputMethodContext::Blur() {
  if (text_input_) {
    text_input_->Deactivate();
    text_input_->HideInputPanel();
  }
}

void WaylandInputMethodContext::SetCursorLocation(const gfx::Rect& rect) {
  if (text_input_)
    text_input_->SetCursorRect(rect);
}

void WaylandInputMethodContext::SetSurroundingText(
    const base::string16& text,
    const gfx::Range& selection_range) {
  if (text_input_)
    text_input_->SetSurroundingText(text, selection_range);
}

void WaylandInputMethodContext::OnPreeditString(const std::string& text,
                                                int preedit_cursor) {
  gfx::Range selection_range = gfx::Range::InvalidRange();

  if (!selection_range.IsValid()) {
    int cursor_pos = (preedit_cursor) ? text.length() : preedit_cursor;
    selection_range.set_start(cursor_pos);
    selection_range.set_end(cursor_pos);
  }

  ui::CompositionText composition_text;
  composition_text.text = base::UTF8ToUTF16(text);
  composition_text.selection = selection_range;
  delegate_->OnPreeditChanged(composition_text);
}

void WaylandInputMethodContext::OnCommitString(const std::string& text) {
  delegate_->OnCommit(base::UTF8ToUTF16(text));
}

void WaylandInputMethodContext::OnDeleteSurroundingText(int32_t index,
                                                        uint32_t length) {
  delegate_->OnDeleteSurroundingText(index, length);
}

void WaylandInputMethodContext::OnKeysym(uint32_t key,
                                         uint32_t state,
                                         uint32_t modifiers) {
  uint8_t flags = 0;  // for now ignore modifiers
  DomKey dom_key = NonPrintableXKeySymToDomKey(key);
  KeyboardCode key_code = NonPrintableDomKeyToKeyboardCode(dom_key);
  DomCode dom_code =
      KeycodeConverter::NativeKeycodeToDomCode(key_code + kXkbKeycodeOffset);
  if (dom_code == ui::DomCode::NONE)
    return;

  bool down = state == WL_KEYBOARD_KEY_STATE_PRESSED;
  ui::KeyEvent event(down ? ET_KEY_PRESSED : ET_KEY_RELEASED, key_code,
                     dom_code, flags, dom_key, EventTimeForNow());
  callback_.Run(&event);
}

}  // namespace ui
