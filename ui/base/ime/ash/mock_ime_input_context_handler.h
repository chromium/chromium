// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_MOCK_IME_INPUT_CONTEXT_HANDLER_H_
#define UI_BASE_IME_ASH_MOCK_IME_INPUT_CONTEXT_HANDLER_H_

#include <stdint.h>

#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "ui/base/ime/ash/text_input_target.h"
#include "ui/base/ime/composition_text.h"
#include "ui/events/event.h"
#include "ui/gfx/range/range.h"

namespace ui {
class InputMethod;
}

namespace ash {

class COMPONENT_EXPORT(UI_BASE_IME_ASH) MockIMEInputContextHandler
    : public TextInputTarget {
 public:
  struct UpdateCompositionTextArg {
    ui::CompositionText composition_text;
    gfx::Range selection;
    bool is_visible;
  };

  struct DeleteSurroundingTextArg {
    uint32_t num_char16s_before_cursor;
    uint32_t num_char16s_after_cursor;
  };

  struct ReplaceSurroundingTextArg {
    uint32_t length_before_selection;
    uint32_t length_after_selection;
    std::u16string replacement_text;
  };

  class Observer : public base::CheckedObserver {
   public:
    // Called whenever the commit text is updated.
    virtual void OnCommitText(const std::u16string& text) = 0;
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  MockIMEInputContextHandler();
  virtual ~MockIMEInputContextHandler();

  void CommitText(
      const std::u16string& text,
      ui::TextInputClient::InsertTextCursorBehavior cursor_behavior) override;
  void UpdateCompositionText(const ui::CompositionText& text,
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
  void SetAutocorrectRange(const gfx::Range& range,
                           SetAutocorrectRangeDoneCallback callback) override;
  bool ClearGrammarFragments(const gfx::Range& range) override;
  std::optional<ui::GrammarFragment> GetGrammarFragmentAtCursor() override;
  bool AddGrammarFragments(
      const std::vector<ui::GrammarFragment>& fragments) override;
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

  std::vector<ui::GrammarFragment> get_grammar_fragments() const {
    return grammar_fragments_;
  }

  void set_cursor_range(gfx::Range range) { cursor_range_ = range; }
  int commit_text_call_count() const { return commit_text_call_count_; }
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

  const ReplaceSurroundingTextArg& last_replace_surrounding_text_arg() const {
    return last_replace_surrounding_text_arg_;
  }

  void set_autocorrect_enabled(bool enabled) {
    autocorrect_enabled_ = enabled;
    if (!enabled) {
      autocorrect_range_ = gfx::Range();
    }
  }

  const std::vector<ui::KeyEvent>& sent_key_events() const {
    return sent_key_events_;
  }

  // Resets all call count.
  void Reset();

 private:
  int commit_text_call_count_;
  int update_preedit_text_call_count_;
  int delete_surrounding_text_call_count_;
  std::u16string last_commit_text_;
  std::vector<ui::KeyEvent> sent_key_events_;
  UpdateCompositionTextArg last_update_composition_arg_;
  DeleteSurroundingTextArg last_delete_surrounding_text_arg_;
  gfx::Range autocorrect_range_;
  bool autocorrect_enabled_ = true;
  std::vector<ui::GrammarFragment> grammar_fragments_;
  gfx::Range cursor_range_;
  ReplaceSurroundingTextArg last_replace_surrounding_text_arg_;
  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // UI_BASE_IME_ASH_MOCK_IME_INPUT_CONTEXT_HANDLER_H_
