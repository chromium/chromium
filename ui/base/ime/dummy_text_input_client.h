// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_DUMMY_TEXT_INPUT_CLIENT_H_
#define UI_BASE_IME_DUMMY_TEXT_INPUT_CLIENT_H_

#include <stddef.h>
#include <stdint.h>

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/ime/text_input_client.h"

namespace ui {

// Dummy implementation of TextInputClient. All functions do nothing.
// TODO(crbug.com/1277388): Replace this class with FakeTextInputClient.
class DummyTextInputClient : public TextInputClient {
 public:
  DummyTextInputClient();
  explicit DummyTextInputClient(TextInputType text_input_type);
  DummyTextInputClient(TextInputType text_input_type,
                       TextInputMode text_input_mode);

  DummyTextInputClient(const DummyTextInputClient&) = delete;
  DummyTextInputClient& operator=(const DummyTextInputClient&) = delete;

  ~DummyTextInputClient() override;

  // Overridden from TextInputClient.
  base::WeakPtr<ui::TextInputClient> AsWeakPtr() override;
  void SetCompositionText(const CompositionText& composition) override;
  size_t ConfirmCompositionText(bool keep_selection) override;
  void ClearCompositionText() override;
  void InsertText(const std::u16string& text,
                  InsertTextCursorBehavior cursor_behavior) override;
  void InsertChar(const KeyEvent& event) override;
  TextInputType GetTextInputType() const override;
  TextInputMode GetTextInputMode() const override;
  base::i18n::TextDirection GetTextDirection() const override;
  int GetTextInputFlags() const override;
  bool CanComposeInline() const override;
  gfx::Rect GetCaretBounds() const override;
  gfx::Rect GetSelectionBoundingBox() const override;
  bool GetCompositionCharacterBounds(size_t index,
                                     gfx::Rect* rect) const override;
  bool HasCompositionText() const override;
  ui::TextInputClient::FocusReason GetFocusReason() const override;
  bool GetTextRange(gfx::Range* range) const override;
  bool GetCompositionTextRange(gfx::Range* range) const override;
  bool GetEditableSelectionRange(gfx::Range* range) const override;
  bool SetEditableSelectionRange(const gfx::Range& range) override;
#if BUILDFLAG(IS_MAC)
  bool DeleteRange(const gfx::Range& range) override;
#endif
  bool GetTextFromRange(const gfx::Range& range,
                        std::u16string* text) const override;
  void OnInputMethodChanged() override;
  bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) override;
  void ExtendSelectionAndDelete(size_t before, size_t after) override;
  void EnsureCaretNotInRect(const gfx::Rect& rect) override;
  bool IsTextEditCommandEnabled(TextEditCommand command) const override;
  void SetTextEditCommandForNextKeyEvent(TextEditCommand command) override;
  ukm::SourceId GetClientSourceForMetrics() const override;
  bool ShouldDoLearning() override;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  bool SetCompositionFromExistingText(
      const gfx::Range& range,
      const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) override;
#endif

#if BUILDFLAG(IS_CHROMEOS)
  gfx::Range GetAutocorrectRange() const override;
  gfx::Rect GetAutocorrectCharacterBounds() const override;
  bool SetAutocorrectRange(const gfx::Range& range) override;
  std::optional<GrammarFragment> GetGrammarFragmentAtCursor() const override;
  bool ClearGrammarFragments(const gfx::Range& range) override;
  bool AddGrammarFragments(
      const std::vector<GrammarFragment>& fragments) override;
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  void GetActiveTextInputControlLayoutBounds(
      std::optional<gfx::Rect>* control_bounds,
      std::optional<gfx::Rect>* selection_bounds) override;
#endif
#if BUILDFLAG(IS_WIN)
  void SetActiveCompositionForAccessibility(
      const gfx::Range& range,
      const std::u16string& active_composition_text,
      bool is_composition_committed) override;
#endif

  int insert_char_count() const { return insert_char_count_; }
  char16_t last_insert_char() const { return last_insert_char_; }
  const std::vector<std::u16string>& insert_text_history() const {
    return insert_text_history_;
  }
  const std::vector<CompositionText>& composition_history() const {
    return composition_history_;
  }
  const std::vector<gfx::Range>& selection_history() const {
    return selection_history_;
  }

  std::vector<GrammarFragment> get_grammar_fragments() const {
    return grammar_fragments_;
  }

  void set_autocorrect_enabled(bool enabled) {
    autocorrect_enabled_ = enabled;
    if (!enabled) {
      autocorrect_range_ = gfx::Range();
    }
  }

  TextInputType text_input_type_;
  TextInputMode text_input_mode_;

 private:
  int insert_char_count_;
  char16_t last_insert_char_;
  std::vector<std::u16string> insert_text_history_;
  std::vector<CompositionText> composition_history_;
  std::vector<gfx::Range> selection_history_;
  gfx::Range autocorrect_range_;
  std::vector<GrammarFragment> grammar_fragments_;
  gfx::Range cursor_range_ = gfx::Range::InvalidRange();
  bool autocorrect_enabled_;

  base::WeakPtrFactory<DummyTextInputClient> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_BASE_IME_DUMMY_TEXT_INPUT_CLIENT_H_
