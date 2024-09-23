// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_LINUX_INPUT_METHOD_AURALINUX_H_
#define UI_BASE_IME_LINUX_INPUT_METHOD_AURALINUX_H_

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/input_method_base.h"
#include "ui/base/ime/linux/linux_input_method_context.h"

namespace ui {

// A ui::InputMethod implementation for Aura on Linux platforms. The
// implementation details are separated to ui::LinuxInputMethodContext
// interface.
class COMPONENT_EXPORT(UI_BASE_IME_LINUX) InputMethodAuraLinux
    : public InputMethodBase,
      public LinuxInputMethodContextDelegate {
 public:
  explicit InputMethodAuraLinux(
      ImeKeyEventDispatcher* ime_key_event_dispatcher);
  InputMethodAuraLinux(const InputMethodAuraLinux&) = delete;
  InputMethodAuraLinux& operator=(const InputMethodAuraLinux&) = delete;
  ~InputMethodAuraLinux() override;

  LinuxInputMethodContext* GetContextForTesting();

  // Overriden from InputMethod.
  ui::EventDispatchDetails DispatchKeyEvent(ui::KeyEvent* event) override;
  void OnTextInputTypeChanged(TextInputClient* client) override;
  void OnCaretBoundsChanged(const TextInputClient* client) override;
  void CancelComposition(const TextInputClient* client) override;
  bool IsCandidatePopupOpen() const override;
  VirtualKeyboardController* GetVirtualKeyboardController() override;

  // Overriden from ui::LinuxInputMethodContextDelegate
  void OnCommit(const std::u16string& text) override;
  void OnConfirmCompositionText(bool keep_selection) override;
  void OnDeleteSurroundingText(size_t before, size_t after) override;
  void OnPreeditChanged(const CompositionText& composition_text) override;
  void OnPreeditEnd() override;
  void OnPreeditStart() override {}
  void OnSetPreeditRegion(const gfx::Range& range,
                          const std::vector<ImeTextSpan>& spans) override;
  void OnClearGrammarFragments(const gfx::Range& range) override;
  void OnAddGrammarFragment(const ui::GrammarFragment& fragment) override;
  void OnSetAutocorrectRange(const gfx::Range& range) override;
  void OnSetVirtualKeyboardOccludedBounds(
      const gfx::Rect& screen_bounds) override;
  void OnInsertImage(const GURL& src) override;

 protected:
  // Overridden from InputMethodBase.
  void OnWillChangeFocusedClient(TextInputClient* focused_before,
                                 TextInputClient* focused) override;
  void OnDidChangeFocusedClient(TextInputClient* focused_before,
                                TextInputClient* focused) override;

 private:
  // Continues to dispatch the EventType::kKeyPressed event to the client.
  // This needs to be called "before" committing the result string or
  // the composition string.
  ui::EventDispatchDetails DispatchImeFilteredKeyPressEvent(
      ui::KeyEvent* event);
  enum class CommitResult {
    kSuccess,          // Successfully committed at least one character.
    kNoCommitString,   // No available string to commit.
    kTargetDestroyed,  // Target was destroyed during the commit.
  };
  CommitResult MaybeCommitResult(bool filtered, const KeyEvent& event);
  bool UpdateCompositionIfTextSelected();
  bool UpdateCompositionIfChanged(bool text_committed);

  // Shared implementation of OnPreeditChanged and OnPreeditEnd.
  // |force_update_client| is designed to dispatch key event/update
  // the client's composition string, specifically for async-mode case.
  void OnPreeditUpdate(const ui::CompositionText& composition_text,
                       bool force_update_client);
  void ConfirmCompositionText(bool keep_selection);
  bool HasInputMethodResult();
  bool NeedInsertChar(const std::optional<std::u16string>& result_text) const;
  [[nodiscard]] ui::EventDispatchDetails SendFakeProcessKeyEvent(
      ui::KeyEvent* event) const;
  void UpdateContextFocusState();
  void ResetContext();
  bool IgnoringNonKeyInput() const;

  std::unique_ptr<LinuxInputMethodContext> context_;

  // The last key event that IME is probably in process in
  // async-mode.
  std::optional<ui::KeyEvent> ime_filtered_key_event_;

  // Tracks last commit result during one key dispatch event.
  std::optional<CommitResult> last_commit_result_;

  std::optional<std::u16string> result_text_;
  std::optional<std::u16string> surrounding_text_;
  gfx::Range text_range_;
  gfx::Range selection_range_;

  ui::CompositionText composition_;

  // The current text input type used to indicates if |context_| and
  // |context_simple_| are focused or not.
  TextInputType text_input_type_;

  // Indicates if currently in sync mode when handling a key event.
  // This is used in OnXXX callbacks from GTK IM module.
  bool is_sync_mode_;

  // Indicates if the composition text is changed or deleted.
  bool composition_changed_;

  // Ignore commit/preedit-changed/preedit-end signals if this time is still in
  // the future.
  base::TimeTicks suppress_non_key_input_until_ = base::TimeTicks::UnixEpoch();

  // Used for making callbacks.
  base::WeakPtrFactory<InputMethodAuraLinux> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_BASE_IME_LINUX_INPUT_METHOD_AURALINUX_H_
