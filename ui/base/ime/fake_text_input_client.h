// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_FAKE_TEXT_INPUT_CLIENT_H_
#define UI_BASE_IME_FAKE_TEXT_INPUT_CLIENT_H_

#include <stddef.h>
#include <stdint.h>

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

class InputMethod;

// Fake in-memory implementation of TextInputClient used for testing.
// This class should act as a 'reference implementation' for TextInputClient.
class FakeTextInputClient : public TextInputClient {
 public:
  struct Options {
    TextInputType type = TEXT_INPUT_TYPE_NONE;
    TextInputMode mode = TEXT_INPUT_MODE_NONE;
    TextInputFlags flags = TEXT_INPUT_FLAG_NONE;
    bool can_insert_image = false;
    bool should_do_learning = false;
    gfx::Rect caret_bounds;
  };

  explicit FakeTextInputClient(TextInputType text_input_type);
  explicit FakeTextInputClient(Options options);
  explicit FakeTextInputClient(InputMethod* input_method, Options options);
  FakeTextInputClient(const FakeTextInputClient& other) = delete;
  FakeTextInputClient& operator=(const FakeTextInputClient& other) = delete;
  ~FakeTextInputClient() override;

  void set_text_input_type(TextInputType text_input_type);
  void set_source_id(ukm::SourceId source_id);
  void SetTextAndSelection(const std::u16string& text, gfx::Range selection);
  void SetFlags(const int flags);
  void SetUrl(const GURL& url);

  const std::u16string& text() const { return text_; }
  const gfx::Range& selection() const { return selection_; }
  const gfx::Range& composition_range() const { return composition_range_; }
  const std::vector<ui::ImeTextSpan>& ime_text_spans() const {
    return ime_text_spans_;
  }
  std::optional<GURL> last_inserted_image_url() const {
    return last_inserted_image_url_;
  }

  // Sets this instance as the focused text input client.
  void Focus();

  // Sets `nullptr` as the focused text input client.
  void Blur();

  // TextInputClient:
  base::WeakPtr<ui::TextInputClient> AsWeakPtr() override;
  void SetCompositionText(const CompositionText& composition) override;
  size_t ConfirmCompositionText(bool keep_selection) override;
  void ClearCompositionText() override;
  void InsertText(
      const std::u16string& text,
      TextInputClient::InsertTextCursorBehavior cursor_behavior) override;
  void InsertChar(const KeyEvent& event) override;
  bool CanInsertImage() override;
  void InsertImage(const GURL& src) override;
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
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
  ui::TextInputClient::EditingContext GetTextEditingContext() override;
#endif

 private:
  raw_ptr<ui::InputMethod> input_method_ = nullptr;
  TextInputType text_input_type_;
  TextInputMode mode_ = TEXT_INPUT_MODE_NONE;
  std::u16string text_;
  gfx::Range selection_;
  gfx::Range composition_range_;
  std::vector<ui::ImeTextSpan> ime_text_spans_;
  gfx::Range autocorrect_range_;
  ukm::SourceId source_id_ = ukm::kInvalidSourceId;
  int flags_ = TEXT_INPUT_FLAG_NONE;
  GURL url_;
  bool can_insert_image_ = false;
  std::optional<GURL> last_inserted_image_url_;
  gfx::Rect caret_bounds_;
  bool should_do_learning_ = false;

  base::WeakPtrFactory<FakeTextInputClient> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_BASE_IME_FAKE_TEXT_INPUT_CLIENT_H_
