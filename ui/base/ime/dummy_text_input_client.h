// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_DUMMY_TEXT_INPUT_CLIENT_H_
#define UI_BASE_IME_DUMMY_TEXT_INPUT_CLIENT_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "build/build_config.h"
#include "ui/base/ime/text_input_client.h"

namespace ui {

// Dummy implementation of TextInputClient. All functions do nothing.
class DummyTextInputClient : public TextInputClient {
 public:
  DummyTextInputClient();
  explicit DummyTextInputClient(TextInputType text_input_type);
  DummyTextInputClient(TextInputType text_input_type,
                       TextInputMode text_input_mode);
  ~DummyTextInputClient() override;

  // Overriden from TextInputClient.
  void SetCompositionText(const CompositionText& composition) override;
  void ConfirmCompositionText(bool) override;
  void ClearCompositionText() override;
  void InsertText(const base::string16& text) override;
  void InsertChar(const KeyEvent& event) override;
  TextInputType GetTextInputType() const override;
  TextInputMode GetTextInputMode() const override;
  base::i18n::TextDirection GetTextDirection() const override;
  int GetTextInputFlags() const override;
  bool CanComposeInline() const override;
  gfx::Rect GetCaretBounds() const override;
  bool GetCompositionCharacterBounds(uint32_t index,
                                     gfx::Rect* rect) const override;
  bool HasCompositionText() const override;
  ui::TextInputClient::FocusReason GetFocusReason() const override;
  bool GetTextRange(gfx::Range* range) const override;
  bool GetCompositionTextRange(gfx::Range* range) const override;
  bool GetEditableSelectionRange(gfx::Range* range) const override;
  bool SetEditableSelectionRange(const gfx::Range& range) override;
  bool DeleteRange(const gfx::Range& range) override;
  bool GetTextFromRange(const gfx::Range& range,
                        base::string16* text) const override;
  void OnInputMethodChanged() override;
  bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) override;
  void ExtendSelectionAndDelete(size_t before, size_t after) override;
  void EnsureCaretNotInRect(const gfx::Rect& rect) override;
  bool IsTextEditCommandEnabled(TextEditCommand command) const override;
  void SetTextEditCommandForNextKeyEvent(TextEditCommand command) override;
  ukm::SourceId GetClientSourceForMetrics() const override;
  bool ShouldDoLearning() override;

#if defined(OS_WIN) || defined(OS_CHROMEOS)
  bool SetCompositionFromExistingText(
      const gfx::Range& range,
      const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) override;
#endif

#if defined(OS_WIN)
  void SetActiveCompositionForAccessibility(
      const gfx::Range& range,
      const base::string16& active_composition_text,
      bool is_composition_committed) override;
#endif

  int insert_char_count() const { return insert_char_count_; }
  base::char16 last_insert_char() const { return last_insert_char_; }
  const std::vector<base::string16>& insert_text_history() const {
    return insert_text_history_;
  }
  const std::vector<CompositionText>& composition_history() const {
    return composition_history_;
  }
  const std::vector<gfx::Range>& selection_history() const {
    return selection_history_;
  }

  TextInputType text_input_type_;
  TextInputMode text_input_mode_;

  DISALLOW_COPY_AND_ASSIGN(DummyTextInputClient);

 private:
  int insert_char_count_;
  base::char16 last_insert_char_;
  std::vector<base::string16> insert_text_history_;
  std::vector<CompositionText> composition_history_;
  std::vector<gfx::Range> selection_history_;
};

}  // namespace ui

#endif  // UI_BASE_IME_DUMMY_TEXT_INPUT_CLIENT_H_
