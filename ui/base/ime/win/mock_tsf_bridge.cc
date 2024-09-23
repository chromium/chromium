// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/win/mock_tsf_bridge.h"

#include "base/check_op.h"
#include "ui/base/ime/text_input_client.h"

namespace ui {

MockTSFBridge::MockTSFBridge()
    : text_input_client_(nullptr),
      ime_key_event_dispatcher_(nullptr),
      tsf_text_store_(nullptr) {}

MockTSFBridge::~MockTSFBridge() = default;

bool MockTSFBridge::CancelComposition() {
  ++cancel_composition_call_count_;
  return true;
}

bool MockTSFBridge::ConfirmComposition() {
  ++confirm_composition_call_count_;
  return true;
}

void MockTSFBridge::OnTextInputTypeChanged(const TextInputClient* client) {
  latest_text_input_type_ = client->GetTextInputType();
}

void MockTSFBridge::OnTextLayoutChanged() {
  ++on_text_layout_changed_;
}

void MockTSFBridge::SetFocusedClient(HWND focused_window,
                                     TextInputClient* client) {
  ++set_focused_client_call_count_;
  focused_window_ = focused_window;
  text_input_client_ = client;
}

void MockTSFBridge::RemoveFocusedClient(TextInputClient* client) {
  ++remove_focused_client_call_count_;
  text_input_client_ = nullptr;
  focused_window_ = nullptr;
}

void MockTSFBridge::SetImeKeyEventDispatcher(
    ImeKeyEventDispatcher* ime_key_event_dispatcher) {
  ime_key_event_dispatcher_ = ime_key_event_dispatcher;
}

void MockTSFBridge::RemoveImeKeyEventDispatcher(
    ImeKeyEventDispatcher* ime_key_event_dispatcher) {
  if (ime_key_event_dispatcher == ime_key_event_dispatcher_) {
    ime_key_event_dispatcher_ = nullptr;
  }
}

Microsoft::WRL::ComPtr<ITfThreadMgr> MockTSFBridge::GetThreadManager() {
  return thread_manager_;
}

TextInputClient* MockTSFBridge::GetFocusedTextInputClient() const {
  return text_input_client_;
}

bool MockTSFBridge::IsInputLanguageCJK() {
  return false;
}

void MockTSFBridge::OnUrlChanged() {}

void MockTSFBridge::Reset() {
  enable_ime_call_count_ = 0;
  disable_ime_call_count_ = 0;
  cancel_composition_call_count_ = 0;
  confirm_composition_call_count_ = 0;
  on_text_layout_changed_ = 0;
  associate_focus_call_count_ = 0;
  set_focused_client_call_count_ = 0;
  remove_focused_client_call_count_ = 0;
  text_input_client_ = nullptr;
  focused_window_ = nullptr;
  latest_text_input_type_ = TEXT_INPUT_TYPE_NONE;
}

}  // namespace ui
