// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/ash/mock_ime_engine_handler.h"

#include "ui/base/ime/text_input_flags.h"

namespace ash {

MockIMEEngineHandler::MockIMEEngineHandler()
    : focus_in_call_count_(0),
      focus_out_call_count_(0),
      set_surrounding_text_call_count_(0),
      process_key_event_call_count_(0),
      reset_call_count_(0),
      last_text_input_context_(ui::TEXT_INPUT_TYPE_NONE) {}

MockIMEEngineHandler::~MockIMEEngineHandler() = default;

void MockIMEEngineHandler::Focus(const InputContext& input_context) {
  last_text_input_context_ = input_context;
  if (last_text_input_context_.type != ui::TEXT_INPUT_TYPE_NONE) {
    ++focus_in_call_count_;
  }
}

void MockIMEEngineHandler::Blur() {
  if (last_text_input_context_.type != ui::TEXT_INPUT_TYPE_NONE) {
    ++focus_out_call_count_;
  }
  last_text_input_context_.type = ui::TEXT_INPUT_TYPE_NONE;
}

void MockIMEEngineHandler::Enable(const std::string& component_id) {}

void MockIMEEngineHandler::Disable() {}

void MockIMEEngineHandler::Reset() {
  ++reset_call_count_;
}

void MockIMEEngineHandler::ProcessKeyEvent(const ui::KeyEvent& key_event,
                                           KeyEventDoneCallback callback) {
  ++process_key_event_call_count_;
  last_processed_key_event_ = std::make_unique<ui::KeyEvent>(key_event);
  last_passed_callback_ = std::move(callback);
}

void MockIMEEngineHandler::SetCaretBounds(const gfx::Rect& caret_bounds) {}

ui::VirtualKeyboardController*
MockIMEEngineHandler::GetVirtualKeyboardController() const {
  return nullptr;
}

void MockIMEEngineHandler::PropertyActivate(const std::string& property_name) {
  last_activated_property_ = property_name;
}

void MockIMEEngineHandler::CandidateClicked(uint32_t index) {}

void MockIMEEngineHandler::AssistiveWindowChanged(
    const ash::ime::AssistiveWindow& window) {}

void MockIMEEngineHandler::SetSurroundingText(const std::u16string& text,
                                              const gfx::Range selection_range,
                                              uint32_t offset_pos) {
  ++set_surrounding_text_call_count_;
  last_set_surrounding_text_ = text;
  last_set_selection_range_ = selection_range;
}

bool MockIMEEngineHandler::IsReadyForTesting() {
  return true;
}

const std::string& MockIMEEngineHandler::GetActiveComponentId() const {
  return active_component_id_;
}

}  // namespace ash
