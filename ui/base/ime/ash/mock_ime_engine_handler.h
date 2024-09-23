// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_MOCK_IME_ENGINE_HANDLER_H_
#define UI_BASE_IME_ASH_MOCK_IME_ENGINE_HANDLER_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/component_export.h"
#include "ui/base/ime/ash/text_input_method.h"
#include "ui/events/event.h"

namespace ash {

namespace ime {
struct AssistiveWindow;
}  // namespace ime

class COMPONENT_EXPORT(UI_BASE_IME_ASH) MockIMEEngineHandler
    : public TextInputMethod {
 public:
  MockIMEEngineHandler();
  ~MockIMEEngineHandler() override;

  // TextInputMethod:
  void Focus(const InputContext& input_context) override;
  void Blur() override;
  void Enable(const std::string& component_id) override;
  void Disable() override;
  void Reset() override;
  void ProcessKeyEvent(const ui::KeyEvent& key_event,
                       KeyEventDoneCallback callback) override;
  void SetCaretBounds(const gfx::Rect& caret_bounds) override;
  ui::VirtualKeyboardController* GetVirtualKeyboardController() const override;
  void PropertyActivate(const std::string& property_name) override;
  void CandidateClicked(uint32_t index) override;
  void AssistiveWindowChanged(const ash::ime::AssistiveWindow& window) override;
  void SetSurroundingText(const std::u16string& text,
                          gfx::Range selection_range,
                          uint32_t offset_pos) override;
  bool IsReadyForTesting() override;

  const std::string& GetActiveComponentId() const;

  int focus_in_call_count() const { return focus_in_call_count_; }
  int focus_out_call_count() const { return focus_out_call_count_; }
  int reset_call_count() const { return reset_call_count_; }
  int set_surrounding_text_call_count() const {
    return set_surrounding_text_call_count_;
  }
  int process_key_event_call_count() const {
    return process_key_event_call_count_;
  }

  const InputContext& last_text_input_context() const {
    return last_text_input_context_;
  }

  std::string last_activated_property() const {
    return last_activated_property_;
  }

  std::u16string last_set_surrounding_text() const {
    return last_set_surrounding_text_;
  }

  const gfx::Range& last_set_selection_range() const {
    return last_set_selection_range_;
  }

  const ui::KeyEvent* last_processed_key_event() const {
    return last_processed_key_event_.get();
  }

  KeyEventDoneCallback last_passed_callback() {
    return std::move(last_passed_callback_);
  }

 private:
  int focus_in_call_count_;
  int focus_out_call_count_;
  int set_surrounding_text_call_count_;
  int process_key_event_call_count_;
  int reset_call_count_;
  InputContext last_text_input_context_;
  std::string last_activated_property_;
  std::u16string last_set_surrounding_text_;
  gfx::Range last_set_selection_range_;
  std::unique_ptr<ui::KeyEvent> last_processed_key_event_;
  KeyEventDoneCallback last_passed_callback_;
  std::string active_component_id_;
};

}  // namespace ash

#endif  // UI_BASE_IME_ASH_MOCK_IME_ENGINE_HANDLER_H_
