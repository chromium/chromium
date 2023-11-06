// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_TEXT_INPUT_METHOD_H_
#define UI_BASE_IME_ASH_TEXT_INPUT_METHOD_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
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
enum class KeyEventHandledState {
  kNotHandled = 0,
  kHandledByIME = 1,
  kHandledByAssistiveSuggester = 2,
  // Same as kNotHandled, except that the autorepeat for this key should be
  // suppressed.
  kNotHandledSuppressAutoRepeat = 3,
};
}  // namespace ime
}  // namespace ui

namespace ash {

namespace ime {
struct AssistiveWindow;
}

enum class PersonalizationMode {
  // The input method MUST not use anything from the input field to update any
  // personalized data (e.g. to improve suggestions quality). Personalization
  // could be disabled if the content is privacy-sensitive (e.g. incognito mode
  // in
  // Chrome browser), or if using personalization does not make sense (e.g.
  // playing a typing game may pollute the dictionary with uncommon words).
  kDisabled,
  // The input method MAY use the input field contents for personalization.
  kEnabled
};

enum class AutocompletionMode { kUnspecified, kDisabled, kEnabled };

enum class AutocorrectionMode { kUnspecified, kDisabled, kEnabled };

enum class SpellcheckMode { kUnspecified, kDisabled, kEnabled };

enum class AutocapitalizationMode {
  kUnspecified,
  kNone,
  kCharacters,
  kWords,
  kSentences,
};

// An interface representing an input method that can read and manipulate text
// in a TextInputTarget. For example, this can represent a Japanese input method
// that can compose and insert Japanese characters into a TextInputTarget.
class COMPONENT_EXPORT(UI_BASE_IME_ASH) TextInputMethod {
 public:
  using KeyEventDoneCallback =
      base::OnceCallback<void(ui::ime::KeyEventHandledState)>;

  // A information about a focused text input field.
  // A type of each member is based on the html spec, but InputContext can be
  // used to specify about a non html text field like Omnibox.
  struct InputContext {
    explicit InputContext(ui::TextInputType type) : type(type) {}

    ui::TextInputType type = ui::TEXT_INPUT_TYPE_NONE;
    ui::TextInputMode mode = ui::TEXT_INPUT_MODE_DEFAULT;
    AutocompletionMode autocompletion_mode = AutocompletionMode::kUnspecified;
    AutocorrectionMode autocorrection_mode = AutocorrectionMode::kUnspecified;
    SpellcheckMode spellcheck_mode = SpellcheckMode::kUnspecified;
    AutocapitalizationMode autocapitalization_mode =
        AutocapitalizationMode::kUnspecified;
    // How this input field was focused.
    ui::TextInputClient::FocusReason focus_reason =
        ui::TextInputClient::FOCUS_REASON_NONE;
    // Whether text entered in this field should be used to improve typing
    // suggestions for the user.
    PersonalizationMode personalization_mode = PersonalizationMode::kDisabled;
  };

  virtual ~TextInputMethod() = default;

  // Informs the input method that an input field has gained focus.
  // `input_context` contains information about the newly focused input field.
  virtual void Focus(const InputContext& input_context) = 0;

  // Informs the input method that focus has been lost.
  virtual void Blur() = 0;

  // Called when the IME is enabled.
  virtual void Enable(const std::string& component_id) = 0;

  // Called when the IME is disabled.
  virtual void Disable() = 0;

  // Called when the IME is reset.
  virtual void Reset() = 0;

  // Called when the key event is received.
  // Actual implementation must call |callback| after key event handling.
  virtual void ProcessKeyEvent(const ui::KeyEvent& key_event,
                               KeyEventDoneCallback callback) = 0;

  // Called when the surrounding text has changed. The |text| is surrounding
  // text and |selection_range| is the range of selection within |text|.
  // |selection_range| has a direction: the start is also called the 'anchor`
  // and the end is also called the 'focus'. If not all surrounding text is
  // given, |offset_pos| indicates the starting offset of |text|.
  virtual void SetSurroundingText(const std::u16string& text,
                                  gfx::Range selection_range,
                                  uint32_t offset_pos) = 0;

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

  // Called when an input's assistive window state is updated.
  virtual void AssistiveWindowChanged(const ime::AssistiveWindow& window) = 0;

  // Returns whether the IME is ready to accept key events for testing.
  virtual bool IsReadyForTesting() = 0;
};

}  // namespace ash

#endif  // UI_BASE_IME_ASH_TEXT_INPUT_METHOD_H_
