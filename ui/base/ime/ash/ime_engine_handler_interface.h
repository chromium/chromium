// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_IME_ENGINE_HANDLER_INTERFACE_H_
#define UI_BASE_IME_ASH_IME_ENGINE_HANDLER_INTERFACE_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "build/build_config.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/events/event_constants.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ui {

class VirtualKeyboardController;
class KeyEvent;

namespace ime {
struct AssistiveWindowButton;
}  // namespace ime

// A interface to handle the engine handler method call.
class COMPONENT_EXPORT(UI_BASE_IME_ASH) IMEEngineHandlerInterface {
 public:
  using KeyEventDoneCallback = base::OnceCallback<void(bool)>;

  // A information about a focused text input field.
  // A type of each member is based on the html spec, but InputContext can be
  // used to specify about a non html text field like Omnibox.
  struct InputContext {
    InputContext(TextInputType type,
                 TextInputMode mode,
                 int flags,
                 TextInputClient::FocusReason focus_reason,
                 bool should_do_learning)
        : type(type),
          mode(mode),
          flags(flags),
          focus_reason(focus_reason),
          should_do_learning(should_do_learning) {}
    TextInputType type;
    TextInputMode mode;
    // Flags for web input fields. Please refer to WebTextInputType.
    int flags;
    // How this input field was focused.
    TextInputClient::FocusReason focus_reason;
    // Whether text entered in this field should be used to improve typing
    // suggestions for the user.
    bool should_do_learning;
  };

  virtual ~IMEEngineHandlerInterface() = default;

  // Called when an input field gains focus.
  virtual void FocusIn(const InputContext& input_context) = 0;

  // Called on touch inside an input field which already has focus.
  virtual void OnTouch(ui::EventPointerType pointerType) = 0;

  // Called when the currently focused input field loses the focus.
  virtual void FocusOut() = 0;

  // Called when the IME is enabled.
  virtual void Enable(const std::string& component_id) = 0;

  // Called when the IME is disabled.
  virtual void Disable() = 0;

  // Called when the IME is reset.
  virtual void Reset() = 0;

  // Called when the key event is received.
  // Actual implementation must call |callback| after key event handling.
  virtual void ProcessKeyEvent(const KeyEvent& key_event,
                               KeyEventDoneCallback callback) = 0;

  // Called when a new surrounding text is set. The |text| is surrounding text
  // and |cursor_pos| is 0 based index of cursor position in |text|. If there is
  // selection range, |anchor_pos| represents opposite index from |cursor_pos|.
  // Otherwise |anchor_pos| is equal to |cursor_pos|. If not all surrounding
  // text is given |offset_pos| indicates the starting offset of |text|.
  virtual void SetSurroundingText(const std::u16string& text,
                                  uint32_t cursor_pos,
                                  uint32_t anchor_pos,
                                  uint32_t offset_pos) = 0;

  // Called when the composition bounds changed.
  virtual void SetCompositionBounds(const std::vector<gfx::Rect>& bounds) = 0;

  // Called when caret bounds changed.
  virtual void SetCaretBounds(const gfx::Rect& caret_bounds) = 0;

  // Gets the implementation of the keyboard controller.
  virtual ui::VirtualKeyboardController* GetVirtualKeyboardController()
      const = 0;

  // Called when a property is activated or changed.
  virtual void PropertyActivate(const std::string& property_name) = 0;

  // Called when the candidate in lookup table is clicked. The |index| is 0
  // based candidate index in lookup table.
  virtual void CandidateClicked(uint32_t index) = 0;

  // Called when assistive window is clicked.
  virtual void AssistiveWindowButtonClicked(
      const ui::ime::AssistiveWindowButton& button) {}

  // Sets the mirroring/casting enable states.
  virtual void SetMirroringEnabled(bool mirroring_enabled) = 0;
  virtual void SetCastingEnabled(bool casting_enabled) = 0;

  // Returns whether the IME is ready to accept key events for testing.
  virtual bool IsReadyForTesting() = 0;

 protected:
  IMEEngineHandlerInterface() = default;
};

}  // namespace ui

#endif  // UI_BASE_IME_ASH_IME_ENGINE_HANDLER_INTERFACE_H_
