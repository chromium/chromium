// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_CHROMEOS_INPUT_METHOD_CHROMEOS_H_
#define UI_BASE_IME_CHROMEOS_INPUT_METHOD_CHROMEOS_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/ime/character_composer.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/ime_input_context_handler_interface.h"
#include "ui/base/ime/input_method_base.h"
#include "ui/base/ime/text_input_client.h"

namespace ui {

class TestableInputMethodChromeOS;

// A ui::InputMethod implementation for ChromeOS.
class COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS) InputMethodChromeOS
    : public InputMethodBase {
 public:
  explicit InputMethodChromeOS(internal::InputMethodDelegate* delegate);
  ~InputMethodChromeOS() override;

  using AckCallback = base::OnceCallback<void(bool)>;

  // Overridden from InputMethod:
  ui::EventDispatchDetails DispatchKeyEvent(ui::KeyEvent* event) override;
  void OnTextInputTypeChanged(const TextInputClient* client) override;
  void OnCaretBoundsChanged(const TextInputClient* client) override;
  void CancelComposition(const TextInputClient* client) override;
  bool IsCandidatePopupOpen() const override;
  InputMethodKeyboardController* GetInputMethodKeyboardController() override;

  // Overridden from InputMethodBase:
  void OnWillChangeFocusedClient(TextInputClient* focused_before,
                                 TextInputClient* focused) override;
  void OnDidChangeFocusedClient(TextInputClient* focused_before,
                                TextInputClient* focused) override;
  bool SetCompositionRange(
      uint32_t before,
      uint32_t after,
      const std::vector<ui::ImeTextSpan>& text_spans) override;

  bool SetSelectionRange(uint32_t start, uint32_t end) override;
  void ConfirmCompositionText(bool reset_engine, bool keep_selection) override;

 protected:
  // Converts |text| into CompositionText.
  void ExtractCompositionText(const CompositionText& text,
                              uint32_t cursor_position,
                              CompositionText* out_composition) const;

  // Process a key returned from the input method.
  virtual ui::EventDispatchDetails ProcessKeyEventPostIME(
      ui::KeyEvent* event,
      bool skip_process_filtered,
      bool handled,
      bool stopped_propagation) WARN_UNUSED_RESULT;

  // Resets context and abandon all pending results and key events.
  // If |reset_engine| is true, a reset signal will be sent to the IME.
  void ResetContext(bool reset_engine = true);

 private:
  class PendingKeyEvent;
  friend TestableInputMethodChromeOS;

  // Representings a pending SetCompositionRange operation.
  struct PendingSetCompositionRange {
    PendingSetCompositionRange(const gfx::Range& range,
                               const std::vector<ui::ImeTextSpan>& text_spans);
    PendingSetCompositionRange(const PendingSetCompositionRange& other);
    ~PendingSetCompositionRange();

    gfx::Range range;
    std::vector<ui::ImeTextSpan> text_spans;
  };

  // Checks the availability of focused text input client and update focus
  // state.
  void UpdateContextFocusState();

  // Processes a key event that was already filtered by the input method.
  // A VKEY_PROCESSKEY may be dispatched to the EventTargets.
  // It returns the result of whether the event has been stopped propagation
  // when dispatching post IME.
  ui::EventDispatchDetails ProcessFilteredKeyPressEvent(ui::KeyEvent* event)
      WARN_UNUSED_RESULT;

  // Processes a key event that was not filtered by the input method.
  ui::EventDispatchDetails ProcessUnfilteredKeyPressEvent(ui::KeyEvent* event)
      WARN_UNUSED_RESULT;

  // Sends input method result caused by the given key event to the focused text
  // input client.
  void ProcessInputMethodResult(ui::KeyEvent* event, bool filtered);

  // Checks if the pending input method result needs inserting into the focused
  // text input client as a single character.
  bool NeedInsertChar() const;

  // Checks if there is pending input method result.
  bool HasInputMethodResult() const;

  // Passes keyevent and executes character composition if necessary. Returns
  // true if character composer comsumes key event.
  bool ExecuteCharacterComposer(const ui::KeyEvent& event);

  // ui::IMEInputContextHandlerInterface overrides:
  void CommitText(const std::string& text) override;
  void UpdateCompositionText(const CompositionText& text,
                             uint32_t cursor_pos,
                             bool visible) override;
  void DeleteSurroundingText(int32_t offset, uint32_t length) override;

  // Hides the composition text.
  void HidePreeditText();

  // Called from the engine when it completes processing.
  void ProcessKeyEventDone(ui::KeyEvent* event, bool is_handled);

  // Returns whether an non-password input field is focused.
  bool IsNonPasswordInputFieldFocused();

  // Returns true if an text input field is focused.
  bool IsInputFieldFocused();

  // Gets the reason how the focused text input client was focused.
  TextInputClient::FocusReason GetClientFocusReason() const;

  // Pending composition text generated by the current pending key event.
  // It'll be sent to the focused text input client as soon as we receive the
  // processing result of the pending key event.
  CompositionText pending_composition_;

  // Pending result text generated by the current pending key event.
  // It'll be sent to the focused text input client as soon as we receive the
  // processing result of the pending key event.
  base::string16 result_text_;

  base::string16 previous_surrounding_text_;
  gfx::Range previous_selection_range_;

  // Indicates if there is an ongoing composition text.
  bool composing_text_;

  // Indicates if the composition text is changed or deleted.
  bool composition_changed_;

  // Indicates whether there is a pending SetCompositionRange operation.
  base::Optional<PendingSetCompositionRange> pending_composition_range_;

  // An object to compose a character from a sequence of key presses
  // including dead key etc.
  CharacterComposer character_composer_;

  // Indicates whether currently is handling a physical key event.
  // This is used in CommitText/UpdateCompositionText/etc.
  bool handling_key_event_;

  // Used for making callbacks.
  base::WeakPtrFactory<InputMethodChromeOS> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InputMethodChromeOS);
};

}  // namespace ui

#endif  // UI_BASE_IME_CHROMEOS_INPUT_METHOD_CHROMEOS_H_
