// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_FAKE_TEXT_INPUT_CLIENT_H_
#define UI_BASE_IME_FAKE_TEXT_INPUT_CLIENT_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/ime/text_input_client.h"

namespace ui {

// Fake in-memory implementation of TextInputClient used for testing.
// This class should act as a 'reference implementation' for TextInputClient.
class FakeTextInputClient : public TextInputClient {
 public:
  explicit FakeTextInputClient(TextInputType text_input_type);
  FakeTextInputClient(const FakeTextInputClient& other) = delete;
  FakeTextInputClient& operator=(const FakeTextInputClient& other) = delete;
  ~FakeTextInputClient() override;

  void set_text_input_type(TextInputType text_input_type);
  void SetTextAndSelection(const base::string16& text, gfx::Range selection);

  const base::string16& text() const { return text_; }
  const gfx::Range& selection() const { return selection_; }
  const gfx::Range& composition_range() const { return composition_range_; }
  const std::vector<ui::ImeTextSpan>& ime_text_spans() const {
    return ime_text_spans_;
  }

  // TextInputClient:
  void SetCompositionText(const CompositionText& composition) override;
  uint32_t ConfirmCompositionText(bool keep_selection) override;
  void ClearCompositionText() override;
  void InsertText(
      const base::string16& text,
      TextInputClient::InsertTextCursorBehavior cursor_behavior) override;
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
#if defined(OS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
  bool SetCompositionFromExistingText(
      const gfx::Range& range,
      const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) override;
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  gfx::Range GetAutocorrectRange() const override;
  gfx::Rect GetAutocorrectCharacterBounds() const override;
  bool SetAutocorrectRange(const gfx::Range& range) override;
#endif
#if defined(OS_WIN)
  void GetActiveTextInputControlLayoutBounds(
      base::Optional<gfx::Rect>* control_bounds,
      base::Optional<gfx::Rect>* selection_bounds) override;
  void SetActiveCompositionForAccessibility(
      const gfx::Range& range,
      const base::string16& active_composition_text,
      bool is_composition_committed) override;
#endif

 private:
  TextInputType text_input_type_;
  base::string16 text_;
  gfx::Range selection_;
  gfx::Range composition_range_;
  std::vector<ui::ImeTextSpan> ime_text_spans_;
};

}  // namespace ui

#endif  // UI_BASE_IME_FAKE_TEXT_INPUT_CLIENT_H_
