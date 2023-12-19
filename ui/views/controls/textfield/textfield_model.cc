// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/textfield/textfield_model.h"

#include <algorithm>
#include <utility>

#include "base/check_op.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/utf16_indexing.h"
#include "ui/views/style/platform_style.h"

namespace {

// Orders ranges decreasing with respect to their min index. This is useful for
// applying text edits such that an edit doesn't offset the positions of later
// edits. It should be reversed when undoing edits.
void order_ranges(std::vector<gfx::Range>* ranges) {
  std::sort(ranges->begin(), ranges->end(), [](const auto& r1, const auto& r2) {
    return r1.GetMin() > r2.GetMin();
  });
}

// Adjusts |position| for the deletion of |ranges|. E.g., if |position| is 10,
// and |ranges| is {{1, 3}, {15, 18}, and {6, 13}}, this will return 4,
// subtracting 2 (3-1), 0 (15>10), and 4 (10-6) for each range respectively.
size_t adjust_position_for_removals(size_t position,
                                    std::vector<gfx::Range> ranges) {
  size_t adjustment = 0;
  for (auto range : ranges)
    adjustment += range.Intersect(gfx::Range(0, position)).length();
  return position - adjustment;
}

}  // namespace

namespace views {

namespace internal {

// Edit holds state information to undo/redo editing changes. Editing operations
// are merged when possible, like when characters are typed in sequence. Calling
// Commit() marks an edit as an independent operation that shouldn't be merged.
class Edit {
 public:
  enum class Type {
    kInsert,
    kDelete,
    kReplace,
  };

  Edit(const Edit&) = delete;
  Edit& operator=(const Edit&) = delete;

  virtual ~Edit() = default;

  // Revert the change made by this edit in |model|.
  void Undo(TextfieldModel* model) {
    // Insertions must be applied in order of increasing indices since |Redo|
    // applies them in decreasing order.
    auto insertion_texts = old_texts_;
    std::reverse(insertion_texts.begin(), insertion_texts.end());
    auto insertion_text_starts = old_text_starts_;
    std::reverse(insertion_text_starts.begin(), insertion_text_starts.end());
    model->ModifyText({{static_cast<uint32_t>(new_text_start_),
                        static_cast<uint32_t>(new_text_end())}},
                      insertion_texts, insertion_text_starts,
                      old_primary_selection_, old_secondary_selections_);
  }

  // Apply the change of this edit to the |model|.
  void Redo(TextfieldModel* model) {
    std::vector<gfx::Range> deletions;
    for (size_t i = 0; i < old_texts_.size(); ++i) {
      deletions.emplace_back(old_text_starts_[i],
                             old_text_starts_[i] + old_texts_[i].length());
    }
    model->ModifyText(deletions, {new_text_}, {new_text_start_},
                      {static_cast<uint32_t>(new_cursor_pos_),
                       static_cast<uint32_t>(new_cursor_pos_)},
                      {});
  }

  // Try to merge the |edit| into this edit and returns true on success. The
  // merged edit will be deleted after redo and should not be reused.
  bool Merge(const Edit* edit) {
    // Don't merge if previous edit is DELETE. This happens when a
    // user deletes characters then hits return. In this case, the
    // delete should be treated as separate edit that can be undone
    // and should not be merged with the replace edit.
    if (type_ != Type::kDelete && edit->force_merge()) {
      MergeReplace(edit);
      return true;
    }
    return mergeable() && edit->mergeable() && DoMerge(edit);
  }

  // Commits the edit and marks as un-mergeable.
  void Commit() { merge_type_ = MergeType::kDoNotMerge; }

 private:
  friend class InsertEdit;
  friend class ReplaceEdit;
  friend class DeleteEdit;

  Edit(Type type,
       MergeType merge_type,
       std::vector<std::u16string> old_texts,
       std::vector<size_t> old_text_starts,
       gfx::Range old_primary_selection,
       std::vector<gfx::Range> old_secondary_selections,
       bool delete_backward,
       size_t new_cursor_pos,
       const std::u16string& new_text,
       size_t new_text_start)
      : type_(type),
        merge_type_(merge_type),
        old_texts_(old_texts),
        old_text_starts_(old_text_starts),
        old_primary_selection_(old_primary_selection),
        old_secondary_selections_(old_secondary_selections),
        delete_backward_(delete_backward),
        new_cursor_pos_(new_cursor_pos),
        new_text_(new_text),
        new_text_start_(new_text_start) {}

  // Each type of edit provides its own specific merge implementation. Assumes
  // |edit| occurs after |this|.
  virtual bool DoMerge(const Edit* edit) = 0;

  Type type() const { return type_; }

  // Can this edit be merged?
  bool mergeable() const { return merge_type_ == MergeType::kMergeable; }

  // Should this edit be forcibly merged with the previous edit?
  bool force_merge() const { return merge_type_ == MergeType::kForceMerge; }

  // Returns the end index of the |new_text_|.
  size_t new_text_end() const { return new_text_start_ + new_text_.length(); }

  // Merge the replace edit into the current edit. This handles the special case
  // where an omnibox autocomplete string is set after a new character is typed.
  void MergeReplace(const Edit* edit) {
    CHECK_EQ(Type::kReplace, edit->type_);
    CHECK_EQ(1U, edit->old_text_starts_.size());
    CHECK_EQ(0U, edit->old_text_starts_[0]);
    CHECK_EQ(0U, edit->new_text_start_);

    // We need to compute the merged edit's |old_texts_| by undoing this edit.
    // Otherwise, |old_texts_| would be the autocompleted text following the
    // user input. E.g., given goo|[gle.com], when the user types 'g', the text
    // updates to goog|[le.com]. If we leave old_texts_ unchanged as 'gle.com',
    // then undoing will result in 'gle.com' instead of 'goo|[gle.com]'
    std::u16string old_texts = edit->old_texts_[0];
    // Remove |new_text_|.
    old_texts.erase(new_text_start_, new_text_.length());
    // Add |old_texts_| in reverse order since we're undoing an edit.
    for (size_t i = old_texts_.size(); i != 0; i--)
      old_texts.insert(old_text_starts_[i - 1], old_texts_[i - 1]);

    merge_type_ = MergeType::kDoNotMerge;
    old_texts_ = {old_texts};
    old_text_starts_ = {0};
    delete_backward_ = false;
    new_cursor_pos_ = edit->new_cursor_pos_;
    new_text_ = edit->new_text_;
    new_text_start_ = 0;
  }

  Type type_;

  // The type of merging allowed.
  MergeType merge_type_;
  // Deleted texts ordered with decreasing indices.
  std::vector<std::u16string> old_texts_;
  // The indices of |old_texts_|.
  std::vector<size_t> old_text_starts_;
  // The text selection ranges prior to the edit. |old_primary_selection_|
  // represents the selection associated with the cursor.
  gfx::Range old_primary_selection_;
  std::vector<gfx::Range> old_secondary_selections_;
  // True if the deletion is made backward.
  bool delete_backward_;
  // New cursor position.
  size_t new_cursor_pos_;
  // Added text.
  std::u16string new_text_;
  // The index of |new_text_|
  size_t new_text_start_;
};

// Insert text at a given position. Assumes 1) no previous selection and 2) the
// insertion is at the cursor, which will advance by the insertion length.
class InsertEdit : public Edit {
 public:
  InsertEdit(bool mergeable, const std::u16string& new_text, size_t at)
      : Edit(Type::kInsert,
             mergeable ? MergeType::kMergeable : MergeType::kDoNotMerge,
             {} /* old_texts */,
             {} /* old_text_starts */,
             {gfx::Range(at, at)} /* old_primary_selection */,
             {} /* old_secondary_selections */,
             false /* delete_backward */,
             at + new_text.length() /* new_cursor_pos */,
             new_text /* new_text */,
             at /* new_text_start */) {}

  // Merge if |edit| is an insertion continuing forward where |this| ended. E.g.
  // If |this| changed "ab|c" to "abX|c", an edit to "abXY|c" can be merged.
  bool DoMerge(const Edit* edit) override {
    // Reject other edit types, and inserts starting somewhere other than where
    // this insert ended.
    if (edit->type() != Type::kInsert ||
        new_text_end() != edit->new_text_start_)
      return false;
    new_text_ += edit->new_text_;
    new_cursor_pos_ = edit->new_cursor_pos_;
    return true;
  }
};

// Delete one or more ranges and do a single insertion. The insertion need not
// be adjacent to the deletions (e.g. drag & drop).
class ReplaceEdit : public Edit {
 public:
  ReplaceEdit(MergeType merge_type,
              std::vector<std::u16string> old_texts,
              std::vector<size_t> old_text_starts,
              gfx::Range old_primary_selection,
              std::vector<gfx::Range> old_secondary_selections,
              bool backward,
              size_t new_cursor_pos,
              const std::u16string& new_text,
              size_t new_text_start)
      : Edit(Type::kReplace,
             merge_type,
             old_texts,
             old_text_starts,
             old_primary_selection,
             old_secondary_selections,
             backward,
             new_cursor_pos,
             new_text,
             new_text_start) {}

  // Merge if |edit| is an insertion or replacement continuing forward where
  // |this| ended. E.g. If |this| changed "a|bc" to "aX|c", edits to "aXY|" or
  // "aXYc" can be merged. Drag and drops are marked kDoNotMerge and should not
  // get here.
  bool DoMerge(const Edit* edit) override {
    // Reject deletions, replacements deleting multiple ranges, and edits
    // inserting or deleting text somewhere other than where this edit ended.
    if (edit->type() == Type::kDelete || edit->old_texts_.size() > 1 ||
        new_text_end() != edit->new_text_start_ ||
        (!edit->old_text_starts_.empty() &&
         new_text_end() != edit->old_text_starts_[0]))
      return false;
    if (edit->old_texts_.size() == 1)
      old_texts_[0] += edit->old_texts_[0];
    new_text_ += edit->new_text_;
    new_cursor_pos_ = edit->new_cursor_pos_;
    return true;
  }
};

// Delete possibly multiple texts.
class DeleteEdit : public Edit {
 public:
  DeleteEdit(bool mergeable,
             std::vector<std::u16string> texts,
             std::vector<size_t> text_starts,
             gfx::Range old_primary_selection,
             std::vector<gfx::Range> old_secondary_selections,
             bool backward,
             size_t new_cursor_pos)
      : Edit(Type::kDelete,
             mergeable ? MergeType::kMergeable : MergeType::kDoNotMerge,
             texts,
             text_starts,
             old_primary_selection,
             old_secondary_selections,
             backward,
             new_cursor_pos,
             std::u16string() /* new_text */,
             0 /* new_text_start */) {}

  // Merge if |edit| is a deletion continuing in the same direction and position
  // where |this| ended. E.g. If |this| changed "ab|c" to "a|c" an edit to "|c"
  // can be merged.
  bool DoMerge(const Edit* edit) override {
    if (edit->type() != Type::kDelete)
      return false;
    // Deletions with selections are marked kDoNotMerge and should not get here.
    DCHECK(old_secondary_selections_.empty());
    DCHECK(old_primary_selection_.is_empty());
    DCHECK(edit->old_secondary_selections_.empty());
    DCHECK(edit->old_primary_selection_.is_empty());

    if (delete_backward_) {
      // Backspace can be merged only with backspace at the same position.
      if (!edit->delete_backward_ ||
          old_text_starts_[0] !=
              edit->old_text_starts_[0] + edit->old_texts_[0].length())
        return false;
      old_text_starts_[0] = edit->old_text_starts_[0];
      old_texts_[0] = edit->old_texts_[0] + old_texts_[0];
      new_cursor_pos_ = edit->new_cursor_pos_;
    } else {
      // Delete can be merged only with delete at the same position.
      if (edit->delete_backward_ ||
          old_text_starts_[0] != edit->old_text_starts_[0])
        return false;
      old_texts_[0] += edit->old_texts_[0];
    }
    return true;
  }
};

}  // namespace internal

namespace {

// Returns the first segment that is visually emphasized. Usually it's used for
// representing the target clause (on Windows). Returns an invalid range if
// there is no such a range.
gfx::Range GetFirstEmphasizedRange(const ui::CompositionText& composition) {
  for (const auto& underline : composition.ime_text_spans) {
    if (underline.thickness == ui::ImeTextSpan::Thickness::kThick)
      return gfx::Range(underline.start_offset, underline.end_offset);
  }
  return gfx::Range::InvalidRange();
}

// Returns a pointer to the kill buffer which holds the text to be inserted on
// executing yank command. Singleton since it needs to be persisted across
// multiple textfields.
// On Mac, the size of the kill ring (no. of buffers) is controlled by
// NSTextKillRingSize, a text system default. However to keep things simple,
// the default kill ring size of 1 (i.e. a single buffer) is assumed.
std::u16string* GetKillBuffer() {
  static base::NoDestructor<std::u16string> kill_buffer;
  DCHECK(base::CurrentUIThread::IsSet());
  return kill_buffer.get();
}

// Helper method to set the kill buffer.
void SetKillBuffer(const std::u16string& buffer) {
  std::u16string* kill_buffer = GetKillBuffer();
  *kill_buffer = buffer;
}

void SelectRangeInCompositionText(gfx::RenderText* render_text,
                                  size_t cursor,
                                  const gfx::Range& range) {
  DCHECK(render_text);
  DCHECK(range.IsValid());
  size_t start = range.GetMin();
  size_t end = range.GetMax();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Swap |start| and |end| so that GetCaretBounds() can always return the same
  // value during conversion.
  // TODO(yusukes): Check if this works for other platforms. If it is, use this
  // on all platforms.
  std::swap(start, end);
#endif
  render_text->SelectRange(gfx::Range(cursor + start, cursor + end));
}

}  // namespace

/////////////////////////////////////////////////////////////////
// TextfieldModel: public

TextfieldModel::Delegate::~Delegate() = default;

TextfieldModel::TextfieldModel(Delegate* delegate)
    : delegate_(delegate),
      render_text_(gfx::RenderText::CreateRenderText()),
      current_edit_(edit_history_.end()) {}

TextfieldModel::~TextfieldModel() {
  ClearEditHistory();
  ClearComposition();
}

bool TextfieldModel::SetText(const std::u16string& new_text,
                             size_t cursor_position) {
  using MergeType = internal::MergeType;
  bool changed = false;
  if (HasCompositionText()) {
    ConfirmCompositionText();
    changed = true;
  }
  if (text() != new_text) {
    if (changed)  // No need to remember composition.
      Undo();
    // If there is a composition text, don't merge with previous edit.
    // Otherwise, force merge the edits.
    ExecuteAndRecordReplace(
        changed ? MergeType::kDoNotMerge : MergeType::kForceMerge,
        {gfx::Range(0, text().length())}, cursor_position, new_text, 0U);
  }
  ClearSelection();
  return changed;
}

void TextfieldModel::Append(const std::u16string& new_text) {
  if (HasCompositionText())
    ConfirmCompositionText();
  size_t save = GetCursorPosition();
  MoveCursor(gfx::LINE_BREAK, render_text_->GetVisualDirectionOfLogicalEnd(),
             gfx::SELECTION_NONE);
  InsertText(new_text);
  render_text_->SetCursorPosition(save);
  ClearSelection();
}

bool TextfieldModel::Delete(bool add_to_kill_buffer) {
  // |add_to_kill_buffer| should never be true for an obscured textfield.
  DCHECK(!add_to_kill_buffer || !render_text_->obscured());

  if (HasCompositionText()) {
    // No undo/redo for composition text.
    CancelCompositionText();
    return true;
  }

  if (HasSelection()) {
    if (add_to_kill_buffer)
      SetKillBuffer(GetSelectedText());
    DeleteSelection();
    return true;
  }
  const size_t cursor_position = GetCursorPosition();
  if (cursor_position < text().length()) {
    size_t next_grapheme_index = render_text_->IndexOfAdjacentGrapheme(
        cursor_position, gfx::CURSOR_FORWARD);
    gfx::Range range_to_delete(cursor_position, next_grapheme_index);
    if (add_to_kill_buffer)
      SetKillBuffer(GetTextFromRange(range_to_delete));
    ExecuteAndRecordDelete({range_to_delete}, true);
    return true;
  }
  return false;
}

bool TextfieldModel::Backspace(bool add_to_kill_buffer) {
  // |add_to_kill_buffer| should never be true for an obscured textfield.
  DCHECK(!add_to_kill_buffer || !render_text_->obscured());

  if (HasCompositionText()) {
    // No undo/redo for composition text.
    CancelCompositionText();
    return true;
  }

  if (HasSelection()) {
    if (add_to_kill_buffer)
      SetKillBuffer(GetSelectedText());
    DeleteSelection();
    return true;
  }
  const size_t cursor_position = GetCursorPosition();
  if (cursor_position > 0) {
    gfx::Range range_to_delete(
        PlatformStyle::RangeToDeleteBackwards(text(), cursor_position));
    if (add_to_kill_buffer)
      SetKillBuffer(GetTextFromRange(range_to_delete));
    ExecuteAndRecordDelete({range_to_delete}, true);
    return true;
  }
  return false;
}

size_t TextfieldModel::GetCursorPosition() const {
  return render_text_->cursor_position();
}

void TextfieldModel::MoveCursor(gfx::BreakType break_type,
                                gfx::VisualCursorDirection direction,
                                gfx::SelectionBehavior selection_behavior) {
  if (HasCompositionText())
    ConfirmCompositionText();
  render_text_->MoveCursor(break_type, direction, selection_behavior);
}

bool TextfieldModel::MoveCursorTo(const gfx::SelectionModel& cursor) {
  if (HasCompositionText()) {
    ConfirmCompositionText();
    // ConfirmCompositionText() updates cursor position. Need to reflect it in
    // the SelectionModel parameter of MoveCursorTo().
    gfx::Range range(render_text_->selection().start(), cursor.caret_pos());
    if (!range.is_empty())
      return render_text_->SelectRange(range);
    return render_text_->SetSelection(
        gfx::SelectionModel(cursor.caret_pos(), cursor.caret_affinity()));
  }
  return render_text_->SetSelection(cursor);
}

bool TextfieldModel::MoveCursorTo(size_t pos) {
  return MoveCursorTo(gfx::SelectionModel(pos, gfx::CURSOR_FORWARD));
}

bool TextfieldModel::MoveCursorTo(const gfx::Point& point, bool select) {
  if (HasCompositionText())
    ConfirmCompositionText();
  return render_text_->MoveCursorToPoint(point, select);
}

std::u16string TextfieldModel::GetSelectedText() const {
  return GetTextFromRange(render_text_->selection());
}

void TextfieldModel::SelectRange(const gfx::Range& range, bool primary) {
  if (HasCompositionText())
    ConfirmCompositionText();
  render_text_->SelectRange(range, primary);
}

void TextfieldModel::SelectSelectionModel(const gfx::SelectionModel& sel) {
  if (HasCompositionText())
    ConfirmCompositionText();
  render_text_->SetSelection(sel);
}

void TextfieldModel::SelectAll(bool reversed) {
  if (HasCompositionText())
    ConfirmCompositionText();
  render_text_->SelectAll(reversed);
}

void TextfieldModel::SelectWord() {
  if (HasCompositionText())
    ConfirmCompositionText();
  render_text_->SelectWord();
}

void TextfieldModel::ClearSelection() {
  if (HasCompositionText())
    ConfirmCompositionText();
  render_text_->ClearSelection();
}

bool TextfieldModel::CanUndo() {
  return edit_history_.size() && current_edit_ != edit_history_.end();
}

bool TextfieldModel::CanRedo() {
  if (edit_history_.empty())
    return false;
  // There is no redo iff the current edit is the last element in the history.
  auto iter = current_edit_;
  return iter == edit_history_.end() ||  // at the top.
         ++iter != edit_history_.end();
}

bool TextfieldModel::Undo() {
  if (!CanUndo())
    return false;
  DCHECK(!HasCompositionText());
  if (HasCompositionText())
    CancelCompositionText();

  std::u16string old = text();
  size_t old_cursor = GetCursorPosition();
  (*current_edit_)->Commit();
  (*current_edit_)->Undo(this);

  if (current_edit_ == edit_history_.begin())
    current_edit_ = edit_history_.end();
  else
    --current_edit_;
  return old != text() || old_cursor != GetCursorPosition();
}

bool TextfieldModel::Redo() {
  if (!CanRedo())
    return false;
  DCHECK(!HasCompositionText());
  if (HasCompositionText())
    CancelCompositionText();

  if (current_edit_ == edit_history_.end())
    current_edit_ = edit_history_.begin();
  else
    ++current_edit_;
  std::u16string old = text();
  size_t old_cursor = GetCursorPosition();
  (*current_edit_)->Redo(this);
  return old != text() || old_cursor != GetCursorPosition();
}

bool TextfieldModel::Cut() {
  if (!HasCompositionText() && HasSelection(true) &&
      !render_text_->obscured()) {
    ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
        .WriteText(GetSelectedText());
    DeleteSelection();
    return true;
  }
  return false;
}

bool TextfieldModel::Copy() {
  if (!HasCompositionText() && HasSelection(true) &&
      !render_text_->obscured()) {
    ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
        .WriteText(GetSelectedText());
    return true;
  }
  return false;
}

bool TextfieldModel::Paste() {
  std::u16string text;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr, &text);
  if (text.empty())
    return false;

  if (render_text()->multiline()) {
    InsertTextInternal(text, false);
    return true;
  }

  // Leading/trailing whitespace is often selected accidentally, and is rarely
  // critical to include (e.g. when pasting into a find bar).  Trim it.  By
  // contrast, whitespace in the middle of the string may need exact
  // preservation to avoid changing the effect (e.g. converting a full-width
  // space to a regular space), so don't call a more aggressive function like
  // CollapseWhitespace().
  base::TrimWhitespace(text, base::TRIM_ALL, &text);
  // If the clipboard contains all whitespace then paste a single space.
  if (text.empty())
    text = u" ";

  InsertTextInternal(text, false);
  return true;
}

bool TextfieldModel::Transpose() {
  if (HasCompositionText() || HasSelection())
    return false;

  size_t cur = GetCursorPosition();
  size_t next = render_text_->IndexOfAdjacentGrapheme(cur, gfx::CURSOR_FORWARD);
  size_t prev =
      render_text_->IndexOfAdjacentGrapheme(cur, gfx::CURSOR_BACKWARD);

  // At the end of the line, the last two characters should be transposed.
  if (cur == text().length()) {
    DCHECK_EQ(cur, next);
    cur = prev;
    prev = render_text_->IndexOfAdjacentGrapheme(prev, gfx::CURSOR_BACKWARD);
  }

  // This happens at the beginning of the line or when the line has less than
  // two graphemes.
  if (gfx::UTF16IndexToOffset(text(), prev, next) != 2)
    return false;

  SelectRange(gfx::Range(prev, next));
  std::u16string text = GetSelectedText();
  std::u16string transposed_text =
      text.substr(cur - prev) + text.substr(0, cur - prev);

  InsertTextInternal(transposed_text, false);
  return true;
}

bool TextfieldModel::Yank() {
  const std::u16string* kill_buffer = GetKillBuffer();
  if (!kill_buffer->empty() || HasSelection()) {
    InsertTextInternal(*kill_buffer, false);
    return true;
  }
  return false;
}

bool TextfieldModel::HasSelection(bool primary_only) const {
  if (primary_only)
    return !render_text_->selection().is_empty();
  return base::ranges::any_of(
      render_text_->GetAllSelections(),
      [](const auto& selection) { return !selection.is_empty(); });
}

void TextfieldModel::DeleteSelection() {
  DCHECK(!HasCompositionText());
  DCHECK(HasSelection());
  ExecuteAndRecordDelete(render_text_->GetAllSelections(), false);
}

void TextfieldModel::DeletePrimarySelectionAndInsertTextAt(
    const std::u16string& new_text,
    size_t position) {
  using MergeType = internal::MergeType;
  if (HasCompositionText())
    CancelCompositionText();
  // We don't use |ExecuteAndRecordReplaceSelection| because that assumes the
  // insertion occurs at the cursor.
  ExecuteAndRecordReplace(MergeType::kDoNotMerge, {render_text_->selection()},
                          position + new_text.length(), new_text, position);
}

std::u16string TextfieldModel::GetTextFromRange(const gfx::Range& range) const {
  return render_text_->GetTextFromRange(range);
}

void TextfieldModel::GetTextRange(gfx::Range* range) const {
  *range = gfx::Range(0, text().length());
}

void TextfieldModel::SetCompositionText(
    const ui::CompositionText& composition) {
  if (HasCompositionText())
    CancelCompositionText();
  else if (HasSelection())
    DeleteSelection();

  if (composition.text.empty())
    return;

  size_t cursor = GetCursorPosition();
  std::u16string new_text = text();
  SetRenderTextText(new_text.insert(cursor, composition.text));
  composition_range_ = gfx::Range(cursor, cursor + composition.text.length());
  // Don't render IME spans with thickness "kNone".
  if (composition.ime_text_spans.size() > 0 &&
      composition.ime_text_spans[0].thickness !=
          ui::ImeTextSpan::Thickness::kNone)
    render_text_->SetCompositionRange(composition_range_);
  else
    render_text_->SetCompositionRange(gfx::Range::InvalidRange());
  gfx::Range emphasized_range = GetFirstEmphasizedRange(composition);
  if (emphasized_range.IsValid()) {
    // This is a workaround due to the lack of support in RenderText to draw
    // a thick underline. In a composition returned from an IME, the segment
    // emphasized by a thick underline usually represents the target clause.
    // Because the target clause is more important than the actual selection
    // range (or caret position) in the composition here we use a selection-like
    // marker instead to show this range.
    // TODO(yukawa, msw): Support thick underlines and remove this workaround.
    SelectRangeInCompositionText(render_text_.get(), cursor, emphasized_range);
  } else if (!composition.selection.is_empty()) {
    SelectRangeInCompositionText(render_text_.get(), cursor,
                                 composition.selection);
  } else {
    render_text_->SetCursorPosition(cursor + composition.selection.end());
  }
}

void TextfieldModel::SetCompositionFromExistingText(const gfx::Range& range) {
  if (range.is_empty() || !gfx::Range(0, text().length()).Contains(range)) {
    ClearComposition();
    return;
  }

  composition_range_ = range;
  render_text_->SetCompositionRange(range);
}

size_t TextfieldModel::ConfirmCompositionText() {
  DCHECK(HasCompositionText());
  std::u16string composition =
      text().substr(composition_range_.start(), composition_range_.length());
  size_t composition_length = composition_range_.length();
  // TODO(oshima): current behavior on ChromeOS is a bit weird and not
  // sure exactly how this should work. Find out and fix if necessary.
  AddOrMergeEditHistory(std::make_unique<internal::InsertEdit>(
      false, composition, composition_range_.start()));
  render_text_->SetCursorPosition(composition_range_.end());
  ClearComposition();
  if (delegate_)
    delegate_->OnCompositionTextConfirmedOrCleared();
  return composition_length;
}

void TextfieldModel::CancelCompositionText() {
  DCHECK(HasCompositionText());
  gfx::Range range = composition_range_;
  ClearComposition();
  std::u16string new_text = text();
  SetRenderTextText(new_text.erase(range.start(), range.length()));
  render_text_->SetCursorPosition(range.start());
  if (delegate_)
    delegate_->OnCompositionTextConfirmedOrCleared();
}

void TextfieldModel::ClearComposition() {
  composition_range_ = gfx::Range::InvalidRange();
  render_text_->SetCompositionRange(composition_range_);
}

void TextfieldModel::GetCompositionTextRange(gfx::Range* range) const {
  *range = composition_range_;
}

bool TextfieldModel::HasCompositionText() const {
  return !composition_range_.is_empty();
}

void TextfieldModel::ClearEditHistory() {
  edit_history_.clear();
  current_edit_ = edit_history_.end();
}

/////////////////////////////////////////////////////////////////
// TextfieldModel: private

void TextfieldModel::InsertTextInternal(const std::u16string& new_text,
                                        bool mergeable) {
  using MergeType = internal::MergeType;
  if (HasCompositionText()) {
    CancelCompositionText();
    ExecuteAndRecordInsert(new_text, mergeable);
  } else if (HasSelection()) {
    ExecuteAndRecordReplaceSelection(
        mergeable ? MergeType::kMergeable : MergeType::kDoNotMerge, new_text);
  } else {
    ExecuteAndRecordInsert(new_text, mergeable);
  }
}

void TextfieldModel::ReplaceTextInternal(const std::u16string& new_text,
                                         bool mergeable) {
  if (HasCompositionText()) {
    CancelCompositionText();
  } else if (!HasSelection()) {
    size_t cursor = GetCursorPosition();
    const gfx::SelectionModel& model = render_text_->selection_model();
    // When there is no selection, the default is to replace the next grapheme
    // with |new_text|. So, need to find the index of next grapheme first.
    size_t next =
        render_text_->IndexOfAdjacentGrapheme(cursor, gfx::CURSOR_FORWARD);
    if (next == model.caret_pos())
      render_text_->SetSelection(model);
    else
      render_text_->SelectRange(gfx::Range(next, model.caret_pos()));
  }
  // Edit history is recorded in InsertText.
  InsertTextInternal(new_text, mergeable);
}

void TextfieldModel::ClearRedoHistory() {
  if (edit_history_.begin() == edit_history_.end())
    return;
  if (current_edit_ == edit_history_.end()) {
    ClearEditHistory();
    return;
  }
  auto delete_start = current_edit_;
  ++delete_start;
  edit_history_.erase(delete_start, edit_history_.end());
}

void TextfieldModel::ExecuteAndRecordDelete(std::vector<gfx::Range> ranges,
                                            bool mergeable) {
  // We need only check replacement_ranges[0] as |delete_backwards_| is
  // irrelevant for multi-range deletions which can't be merged anyways.
  const bool backward = ranges[0].is_reversed();
  order_ranges(&ranges);

  std::vector<std::u16string> old_texts;
  std::vector<size_t> old_text_starts;
  for (const auto& range : ranges) {
    old_texts.push_back(GetTextFromRange(range));
    old_text_starts.push_back(range.GetMin());
  }

  size_t cursor_pos = adjust_position_for_removals(GetCursorPosition(), ranges);

  auto edit = std::make_unique<internal::DeleteEdit>(
      mergeable, old_texts, old_text_starts, render_text_->selection(),
      render_text_->secondary_selections(), backward, cursor_pos);
  edit->Redo(this);
  AddOrMergeEditHistory(std::move(edit));
}

void TextfieldModel::ExecuteAndRecordReplaceSelection(
    internal::MergeType merge_type,
    const std::u16string& new_text) {
  auto replacement_ranges = render_text_->GetAllSelections();
  size_t new_text_start =
      adjust_position_for_removals(GetCursorPosition(), replacement_ranges);
  size_t new_cursor_pos = new_text_start + new_text.length();

  ExecuteAndRecordReplace(merge_type, replacement_ranges, new_cursor_pos,
                          new_text, new_text_start);
}

void TextfieldModel::ExecuteAndRecordReplace(
    internal::MergeType merge_type,
    std::vector<gfx::Range> replacement_ranges,
    size_t new_cursor_pos,
    const std::u16string& new_text,
    size_t new_text_start) {
  // We need only check replacement_ranges[0] as |delete_backwards_| is
  // irrelevant for multi-range deletions which can't be merged anyways.
  const bool backward = replacement_ranges[0].is_reversed();
  order_ranges(&replacement_ranges);

  std::vector<std::u16string> old_texts;
  std::vector<size_t> old_text_starts;
  for (const auto& range : replacement_ranges) {
    old_texts.push_back(GetTextFromRange(range));
    old_text_starts.push_back(range.GetMin());
  }

  auto edit = std::make_unique<internal::ReplaceEdit>(
      merge_type, old_texts, old_text_starts, render_text_->selection(),
      render_text_->secondary_selections(), backward, new_cursor_pos, new_text,
      new_text_start);
  edit->Redo(this);
  AddOrMergeEditHistory(std::move(edit));
}

void TextfieldModel::ExecuteAndRecordInsert(const std::u16string& new_text,
                                            bool mergeable) {
  auto edit = std::make_unique<internal::InsertEdit>(mergeable, new_text,
                                                     GetCursorPosition());
  edit->Redo(this);
  AddOrMergeEditHistory(std::move(edit));
}

void TextfieldModel::AddOrMergeEditHistory(
    std::unique_ptr<internal::Edit> edit) {
  ClearRedoHistory();

  if (current_edit_ != edit_history_.end() &&
      (*current_edit_)->Merge(edit.get())) {
    // If the new edit was successfully merged with an old one, don't add it to
    // the history.
    return;
  }
  edit_history_.push_back(std::move(edit));
  if (current_edit_ == edit_history_.end()) {
    // If there is no redoable edit, this is the 1st edit because RedoHistory
    // has been already deleted.
    DCHECK_EQ(1u, edit_history_.size());
    current_edit_ = edit_history_.begin();
  } else {
    ++current_edit_;
  }
}

void TextfieldModel::ModifyText(
    const std::vector<gfx::Range>& deletions,
    const std::vector<std::u16string>& insertion_texts,
    const std::vector<size_t>& insertion_positions,
    const gfx::Range& primary_selection,
    const std::vector<gfx::Range>& secondary_selections) {
  DCHECK_EQ(insertion_texts.size(), insertion_positions.size());
  std::u16string old_text = text();
  ClearComposition();

  for (auto deletion : deletions)
    old_text.erase(deletion.start(), deletion.length());
  for (size_t i = 0; i < insertion_texts.size(); ++i)
    old_text.insert(insertion_positions[i], insertion_texts[i]);
  SetRenderTextText(old_text);

  if (primary_selection.start() == primary_selection.end())
    render_text_->SetCursorPosition(primary_selection.start());
  else
    render_text_->SelectRange(primary_selection);
  for (auto secondary_selection : secondary_selections)
    render_text_->SelectRange(secondary_selection, false);
}

void TextfieldModel::SetRenderTextText(const std::u16string& text) {
  render_text_->SetText(text);
  if (delegate_)
    delegate_->OnTextChanged();
}

// static
void TextfieldModel::ClearKillBuffer() {
  SetKillBuffer(std::u16string());
}

}  // namespace views
