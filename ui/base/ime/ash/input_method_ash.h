// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_INPUT_METHOD_ASH_H_
#define UI_BASE_IME_ASH_INPUT_METHOD_ASH_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <string_view>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/ime/ash/text_input_method.h"
#include "ui/base/ime/ash/text_input_target.h"
#include "ui/base/ime/ash/typing_session_manager.h"
#include "ui/base/ime/character_composer.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/input_method_base.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event_dispatcher.h"

namespace ui::ime {
enum class KeyEventHandledState;
}

namespace ash {

// A `ui::InputMethod` implementation for Ash.
class COMPONENT_EXPORT(UI_BASE_IME_ASH) InputMethodAsh
    : public ui::InputMethodBase,
      public TextInputTarget {
 public:
  explicit InputMethodAsh(ui::ImeKeyEventDispatcher* ime_key_event_dispatcher);

  InputMethodAsh(const InputMethodAsh&) = delete;
  InputMethodAsh& operator=(const InputMethodAsh&) = delete;

  ~InputMethodAsh() override;

  // Overridden from ui::InputMethod:
  ui::EventDispatchDetails DispatchKeyEvent(ui::KeyEvent* event) override;
  void OnTextInputTypeChanged(ui::TextInputClient* client) override;
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override;
  void CancelComposition(const ui::TextInputClient* client) override;
  bool IsCandidatePopupOpen() const override;
  ui::VirtualKeyboardController* GetVirtualKeyboardController() override;

  // Overridden from InputMethodBase:
  void OnFocus() override;
  void OnBlur() override;
  void OnWillChangeFocusedClient(ui::TextInputClient* focused_before,
                                 ui::TextInputClient* focused) override;
  void OnDidChangeFocusedClient(ui::TextInputClient* focused_before,
                                ui::TextInputClient* focused) override;

  // TextInputTarget overrides:
  void CommitText(
      const std::u16string& text,
      ui::TextInputClient::InsertTextCursorBehavior cursor_behavior) override;
  bool SetCompositionRange(
      uint32_t before,
      uint32_t after,
      const std::vector<ui::ImeTextSpan>& text_spans) override;
  bool SetComposingRange(
      uint32_t start,
      uint32_t end,
      const std::vector<ui::ImeTextSpan>& text_spans) override;
  gfx::Range GetAutocorrectRange() override;
  void SetAutocorrectRange(const gfx::Range& range,
                           SetAutocorrectRangeDoneCallback callback) override;
  std::optional<ui::GrammarFragment> GetGrammarFragmentAtCursor() override;
  bool ClearGrammarFragments(const gfx::Range& range) override;
  bool AddGrammarFragments(
      const std::vector<ui::GrammarFragment>& fragments) override;
  void UpdateCompositionText(const ui::CompositionText& text,
                             uint32_t cursor_pos,
                             bool visible) override;
  void DeleteSurroundingText(uint32_t num_char16s_before_cursor,
                             uint32_t num_char16s_after_cursor) override;
  void ReplaceSurroundingText(uint32_t length_before_selection,
                              uint32_t length_after_selection,
                              std::u16string_view replacement_text) override;
  SurroundingTextInfo GetSurroundingTextInfo() override;
  void SendKeyEvent(ui::KeyEvent* event) override;
  ui::InputMethod* GetInputMethod() override;
  void ConfirmComposition(bool reset_engine) override;
  bool HasCompositionText() override;
  ukm::SourceId GetClientSourceForMetrics() override;

 protected:
  // Converts `text` into `ui::CompositionText`.
  ui::CompositionText ExtractCompositionText(const ui::CompositionText& text,
                                             uint32_t cursor_position) const;

  // Process a key returned from the input method.
  [[nodiscard]] virtual ui::EventDispatchDetails ProcessKeyEventPostIME(
      ui::KeyEvent* event,
      ui::ime::KeyEventHandledState handled_state,
      bool stopped_propagation);

  // Resets context and abandon all pending results and key events.
  // If |reset_engine| is true, a reset signal will be sent to the IME.
  void ResetContext(bool reset_engine = true);

 private:
  friend class TestableInputMethodAsh;

  // Representings a pending SetCompositionRange operation.
  struct PendingSetCompositionRange {
    PendingSetCompositionRange(const gfx::Range& range,
                               const std::vector<ui::ImeTextSpan>& text_spans);
    PendingSetCompositionRange(const PendingSetCompositionRange& other);
    ~PendingSetCompositionRange();

    gfx::Range range;
    std::vector<ui::ImeTextSpan> text_spans;
  };

  // Representings a pending CommitText operation.
  struct PendingCommit {
    std::u16string text;

    // Where the cursor should be placed in |text|.
    // 0 <= |cursor| <= |text.length()|.
    size_t cursor = 0;
  };

  struct PendingAutocorrectRange {
    PendingAutocorrectRange(const gfx::Range& range,
                            SetAutocorrectRangeDoneCallback callback);
    ~PendingAutocorrectRange();

    gfx::Range range;
    SetAutocorrectRangeDoneCallback callback;
  };

  // Checks the availability of focused text input client and update focus
  // state.
  void UpdateContextFocusState();

  // Processes a key event that was already filtered by the input method.
  // A VKEY_PROCESSKEY may be dispatched to the EventTargets.
  // It returns the result of whether the event has been stopped propagation
  // when dispatching post IME.
  [[nodiscard]] ui::EventDispatchDetails ProcessFilteredKeyPressEvent(
      ui::KeyEvent* event,
      bool only_dispatch_vkey_processkey);

  // Processes a key event that was not filtered by the input method.
  [[nodiscard]] ui::EventDispatchDetails ProcessUnfilteredKeyPressEvent(
      ui::KeyEvent* event);

  // Processes any pending input method operations that issued while handling
  // the key event. Does not do anything if there were no pending operations.
  void MaybeProcessPendingInputMethodResult(ui::KeyEvent* event, bool filtered);

  // Checks if the pending input method result needs inserting into the focused
  // text input client as a single character.
  bool NeedInsertChar() const;

  // Checks if there is pending input method result.
  bool HasInputMethodResult() const;

  // Passes keyevent and executes character composition if necessary. Returns
  // true if character composer comsumes key event.
  bool ExecuteCharacterComposer(const ui::KeyEvent& event);

  // Hides the composition text.
  void HidePreeditText();

  TextInputMethod::InputContext GetInputContext() const;

  // Called from the engine when it completes processing.
  void ProcessKeyEventDone(ui::KeyEvent* event,
                           ui::ime::KeyEventHandledState handled_state);

  bool IsPasswordOrNoneInputFieldFocused();

  // Sends a fake key event for IME composing without physical key events.
  // Returns true if the faked key event is stopped propagation.
  bool SendFakeProcessKeyEvent(bool pressed) const;

  // Pending composition text generated by the current pending key event.
  // It'll be sent to the focused text input client as soon as we receive the
  // processing result of the pending key event.
  std::optional<ui::CompositionText> pending_composition_;

  // Pending result text generated by the current pending key event.
  // It'll be sent to the focused text input client as soon as we receive the
  // processing result of the pending key event.
  std::optional<PendingCommit> pending_commit_;

  std::u16string previous_surrounding_text_;
  gfx::Range previous_selection_range_;

  // Indicates if there is an ongoing composition text.
  bool composing_text_ = false;

  // Indicates if the composition text is changed or deleted.
  bool composition_changed_ = false;

  // Indicates whether there is a pending SetCompositionRange operation.
  std::optional<PendingSetCompositionRange> pending_composition_range_;

  std::unique_ptr<PendingAutocorrectRange> pending_autocorrect_range_;

  // An object to compose a character from a sequence of key presses
  // including dead key etc.
  ui::CharacterComposer character_composer_;

  // Indicates whether currently is handling a physical key event.
  // This is used in CommitText/UpdateCompositionText/etc.
  bool handling_key_event_ = false;

  TypingSessionManager typing_session_manager_;

  // Use by `DispatchKeyEvent` to return a proper event dispatch details
  // when IME engine's `ProcessKeyEvent` invokes `ProcessKeyEventDone`
  // synchronously.
  std::optional<ui::EventDispatchDetails> dispatch_details_;

  // The URL that hosts the currently focused input field.
  // This can be invalid if the URL is not known (e.g. when using an
  // ARC++ app) or there's no focused input field.
  GURL focused_url_;

  // Used for making callbacks.
  base::WeakPtrFactory<InputMethodAsh> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // UI_BASE_IME_ASH_INPUT_METHOD_ASH_H_
