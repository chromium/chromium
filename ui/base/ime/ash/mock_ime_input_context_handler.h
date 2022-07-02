// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_MOCK_IME_INPUT_CONTEXT_HANDLER_H_
#define UI_BASE_IME_ASH_MOCK_IME_INPUT_CONTEXT_HANDLER_H_

#include <stdint.h>

#include <string>

#include "base/component_export.h"
#include "ui/base/ime/ash/ime_input_context_handler_interface.h"
#include "ui/base/ime/composition_text.h"
#include "ui/events/event.h"
#include "ui/gfx/range/range.h"

namespace ui {
class InputMethod;

class COMPONENT_EXPORT(UI_BASE_IME_ASH) MockIMEInputContextHandler
    : public IMEInputContextHandlerInterface {
 public:
  struct UpdateCompositionTextArg {
    CompositionText composition_text;
    gfx::Range selection;
    bool is_visible;
  };

  struct DeleteSurroundingTextArg {
    int32_t offset;
    uint32_t length;
  };

  MockIMEInputContextHandler();
  virtual ~MockIMEInputContextHandler();

  void CommitText(
      const std::u16string& text,
      TextInputClient::InsertTextCursorBehavior cursor_behavior) override;
  void UpdateCompositionText(const CompositionText& text,
                             uint32_t cursor_pos,
                             bool visible) override;

  bool SetCompositionRange(
      uint32_t before,
      uint32_t after,
      const std::vector<ui::ImeTextSpan>& text_spans) override;
  bool SetComposingRange(
      uint32_t start,
      uint32_t end,
      const std::vector<ui::ImeTextSpan>& text_spans) override;
  gfx::Range GetAutocorrectRange() override;
  gfx::Rect GetAutocorrectCharacterBounds() override;
  gfx::Rect GetTextFieldBounds() override;
  bool SetAutocorrectRange(const gfx::Range& range) override;
  bool ClearGrammarFragments(const gfx::Range& range) override;
  absl::optional<GrammarFragment> GetGrammarFragmentAtCursor() override;
  bool AddGrammarFragments(
      const std::vector<GrammarFragment>& fragments) override;
  bool SetSelectionRange(uint32_t start, uint32_t end) override;
  void DeleteSurroundingText(int32_t offset, uint32_t length) override;
  SurroundingTextInfo GetSurroundingTextInfo() override;
  void SendKeyEvent(KeyEvent* event) override;
  InputMethod* GetInputMethod() override;
  void ConfirmCompositionText(bool reset_engine, bool keep_selection) override;
  bool HasCompositionText() override;
  std::u16string GetCompositionText() override;
  ukm::SourceId GetClientSourceForMetrics() override;

  std::vector<GrammarFragment> get_grammar_fragments() const {
    return grammar_fragments_;
  }

  void set_cursor_range(gfx::Range range) { cursor_range_ = range; }
  int commit_text_call_count() const { return commit_text_call_count_; }
  int set_selection_range_call_count() const {
    return set_selection_range_call_count_;
  }
  int update_preedit_text_call_count() const {
    return update_preedit_text_call_count_;
  }

  int delete_surrounding_text_call_count() const {
    return delete_surrounding_text_call_count_;
  }

  int send_key_event_call_count() const { return sent_key_events_.size(); }

  const std::u16string& last_commit_text() const { return last_commit_text_; }

  const UpdateCompositionTextArg& last_update_composition_arg() const {
    return last_update_composition_arg_;
  }

  const DeleteSurroundingTextArg& last_delete_surrounding_text_arg() const {
    return last_delete_surrounding_text_arg_;
  }

  const std::vector<ui::KeyEvent>& sent_key_events() const {
    return sent_key_events_;
  }

  // Resets all call count.
  void Reset();

 private:
  int commit_text_call_count_;
  int set_selection_range_call_count_;
  int update_preedit_text_call_count_;
  int delete_surrounding_text_call_count_;
  std::u16string last_commit_text_;
  std::vector<ui::KeyEvent> sent_key_events_;
  UpdateCompositionTextArg last_update_composition_arg_;
  DeleteSurroundingTextArg last_delete_surrounding_text_arg_;
  gfx::Range autocorrect_range_;
  std::vector<GrammarFragment> grammar_fragments_;
  gfx::Range cursor_range_;
};
}  // namespace ui

#endif  // UI_BASE_IME_ASH_MOCK_IME_INPUT_CONTEXT_HANDLER_H_
