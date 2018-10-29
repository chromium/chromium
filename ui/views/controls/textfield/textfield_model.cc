// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/textfield/textfield_model.h"

#include <algorithm>

#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/utf16_indexing.h"
#include "ui/views/style/platform_style.h"

namespace views {

namespace internal {

// Edit holds state information to undo/redo editing changes. Editing operations
// are merged when possible, like when characters are typed in sequence. Calling
// Commit() marks an edit as an independent operation that shouldn't be merged.
class Edit {
 public:
  enum Type {
    INSERT_EDIT,
    DELETE_EDIT,
    REPLACE_EDIT,
  };

  virtual ~Edit() {}

  // Revert the change made by this edit in |model|.
  void Undo(TextfieldModel* model) {
    model->ModifyText(new_text_start_, new_text_end(), old_text_,
                      old_text_start_, old_selection_);
  }

  // Apply the change of this edit to the |model|.
  void Redo(TextfieldModel* model) {
    model->ModifyText(old_text_start_, old_text_end(), new_text_,
                      new_text_start_,
                      gfx::Range(new_cursor_pos_, new_cursor_pos_));
  }

  // Try to merge the |edit| into this edit and returns true on success. The
  // merged edit will be deleted after redo and should not be reused.
  bool Merge(const Edit* edit) {
    // Don't merge if previous edit is DELETE. This happens when a
    // user deletes characters then hits return. In this case, the
    // delete should be treated as separate edit that can be undone
    // and should not be merged with the replace edit.
    if (type_ != DELETE_EDIT && edit->force_merge()) {
      MergeReplace(edit);
      return true;
    }
    return mergeable() && edit->mergeable() && DoMerge(edit);
  }

  // Commits the edit and marks as un-mergeable.
  void Commit() { merge_type_ = DO_NOT_MERGE; }

 private:
  friend class InsertEdit;
  friend class ReplaceEdit;
  friend class DeleteEdit;

  Edit(Type type,
       MergeType merge_type,
       const base::string16& old_text,
       size_t old_text_start,
       gfx::Range old_selection,
       bool delete_backward,
       size_t new_cursor_pos,
       const base::string16& new_text,
       size_t new_text_start)
      : type_(type),
        merge_type_(merge_type),
        old_text_(old_text),
        old_text_start_(old_text_start),
        old_selection_(old_selection),
        delete_backward_(delete_backward),
        new_cursor_pos_(new_cursor_pos),
        new_text_(new_text),
        new_text_start_(new_text_start) {}

  // Each type of edit provides its own specific merge implementation.
  virtual bool DoMerge(const Edit* edit) = 0;

  Type type() const { return type_; }

  // Can this edit be merged?
  bool mergeable() const { return merge_type_ == MERGEABLE; }

  // Should this edit be forcibly merged with the previous edit?
  bool force_merge() const { return merge_type_ == FORCE_MERGE; }

  // Returns the end index of the |old_text_|.
  size_t old_text_end() const { return old_text_start_ + old_text_.length(); }

  // Returns the end index of the |new_text_|.
  size_t new_text_end() const { return new_text_start_ + new_text_.length(); }

  // Merge the replace edit into the current edit. This handles the special case
  // where an omnibox autocomplete string is set after a new character is typed.
  void MergeReplace(const Edit* edit) {
    CHECK_EQ(REPLACE_EDIT, edit->type_);
    CHECK_EQ(0U, edit->old_text_start_);
    CHECK_EQ(0U, edit->new_text_start_);
    base::string16 old_text = edit->old_text_;
    old_text.erase(new_text_start_, new_text_.length());
    old_text.insert(old_text_start_, old_text_);
    // SetText() replaces entire text. Set |old_text_| to the entire
    // replaced text with |this| edit undone.
    old_text_ = old_text;
    old_text_start_ = edit->old_text_start_;
    delete_backward_ = false;

    new_text_ = edit->new_text_;
    new_text_start_ = edit->new_text_start_;
    merge_type_ = DO_NOT_MERGE;
  }

  Type type_;

  // The type of merging allowed.
  MergeType merge_type_;
  // Deleted text by this edit.
  base::string16 old_text_;
  // The index of |old_text_|.
  size_t old_text_start_;
  // The range of the text selection prior to the edit.
  gfx::Range old_selection_;
  // True if the deletion is made backward.
  bool delete_backward_;
  // New cursor position.
  size_t new_cursor_pos_;
  // Added text.
  base::string16 new_text_;
  // The index of |new_text_|
  size_t new_text_start_;

  DISALLOW_COPY_AND_ASSIGN(Edit);
};

class InsertEdit : public Edit {
 public:
  InsertEdit(bool mergeable, const base::string16& new_text, size_t at)
      : Edit(INSERT_EDIT,
             mergeable ? MERGEABLE : DO_NOT_MERGE,
             base::string16(),
             at,
             gfx::Range(at, at),
             false /* N/A */,
             at + new_text.length() /* new cursor */,
             new_text,
             at) {}

  // Edit implementation.
  bool DoMerge(const Edit* edit) override {
    if (edit->type() != INSERT_EDIT || new_text_end() != edit->new_text_start_)
      return false;
    // If continuous edit, merge it.
    // TODO(oshima): gtk splits edits between whitespace. Find out what
    // we want to here and implement if necessary.
    new_text_ += edit->new_text_;
    new_cursor_pos_ = edit->new_cursor_pos_;
    return true;
  }
};

class ReplaceEdit : public Edit {
 public:
  ReplaceEdit(MergeType merge_type,
              const base::string16& old_text,
              size_t old_text_start,
              gfx::Range old_selection,
              bool backward,
              size_t new_cursor_pos,
              const base::string16& new_text,
              size_t new_text_start)
      : Edit(REPLACE_EDIT,
             merge_type,
             old_text,
             old_text_start,
             old_selection,
             backward,
             new_cursor_pos,
             new_text,
             new_text_start) {}

  // Edit implementation.
  bool DoMerge(const Edit* edit) override {
    if (edit->type() == DELETE_EDIT ||
        new_text_end() != edit->old_text_start_ ||
        edit->old_text_start_ != edit->new_text_start_)
      return false;
    old_text_ += edit->old_text_;
    new_text_ += edit->new_text_;
    new_cursor_pos_ = edit->new_cursor_pos_;
    return true;
  }
};

class DeleteEdit : public Edit {
 public:
  DeleteEdit(bool mergeable,
             const base::string16& text,
             size_t text_start,
             bool backward,
             gfx::Range old_selection)
      : Edit(DELETE_EDIT,
             mergeable ? MERGEABLE : DO_NOT_MERGE,
             text,
             text_start,
             old_selection,
             backward,
             text_start,
             base::string16(),
             text_start) {}

  // Edit implementation.
  bool DoMerge(const Edit* edit) override {
    if (edit->type() != DELETE_EDIT)
      return false;

    if (delete_backward_) {
      // backspace can be merged only with backspace at the same position.
      if (!edit->delete_backward_ || old_text_start_ != edit->old_text_end())
        return false;
      old_text_start_ = edit->old_text_start_;
      old_text_ = edit->old_text_ + old_text_;
      new_cursor_pos_ = edit->new_cursor_pos_;
    } else {
      // delete can be merged only with delete at the same position.
      if (edit->delete_backward_ || old_text_start_ != edit->old_text_start_)
        return false;
      old_text_ += edit->old_text_;
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
  for (size_t i = 0; i < composition.ime_text_spans.size(); ++i) {
    const ui::ImeTextSpan& underline = composition.ime_text_spans[i];
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
base::string16* GetKillBuffer() {
  static base::NoDestructor<base::string16> kill_buffer;
  DCHECK(base::MessageLoopForUI::IsCurrent());
  return kill_buffer.get();
}

// Helper method to set the kill buffer.
void SetKillBuffer(const base::string16& buffer) {
  base::string16* kill_buffer = GetKillBuffer();
  *kill_buffer = buffer;
}

void SelectRangeInCompositionText(gfx::RenderText* render_text,
                                  size_t cursor,
                                  const gfx::Range& range) {
  DCHECK(render_text);
  DCHECK(range.IsValid());
  uint32_t start = range.GetMin();
  uint32_t end = range.GetMax();
#if defined(OS_CHROMEOS)
  // Swap |start| and |end| so that GetCaretBounds() can always return the same
  // value during conversion.
  // TODO(yusukes): Check if this works for other platforms. If it is, use this
  // on all platforms.
  std::swap(start, end);
#endif
  render_text->SelectRange(gfx::Range(cursor + start, cursor + end));
}

}  // namespace

using internal::Edit;
using internal::DeleteEdit;
using internal::InsertEdit;
using internal::ReplaceEdit;
using internal::MergeType;
using internal::DO_NOT_MERGE;
using internal::FORCE_MERGE;
using internal::MERGEABLE;

/////////////////////////////////////////////////////////////////
// TextfieldModel: public

TextfieldModel::Delegate::~Delegate() {}

TextfieldModel::TextfieldModel(Delegate* delegate)
    : delegate_(delegate),
      render_text_(gfx::RenderText::CreateHarfBuzzInstance()),
      current_edit_(edit_history_.end()) {}

TextfieldModel::~TextfieldModel() {
  ClearEditHistory();
  ClearComposition();
}

bool TextfieldModel::SetText(const base::string16& new_text) {
  bool changed = false;
  if (HasCompositionText()) {
    ConfirmCompositionText();
    changed = true;
  }
  if (text() != new_text) {
    if (changed)  // No need to remember composition.
      Undo();
    // SetText moves the cursor to the end.
    size_t new_cursor = new_text.length();
    // If there is a composition text, don't merge with previous edit.
    // Otherwise, force merge the edits.
    ExecuteAndRecordReplace(changed ? DO_NOT_MERGE : FORCE_MERGE,
                            gfx::Range(0, text().length()), new_cursor,
                            new_text, 0U);
    render_text_->SetCursorPosition(new_cursor);
  }
  ClearSelection();
  return changed;
}

void TextfieldModel::Append(const base::string16& new_text) {
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
  if (text().length() > GetCursorPosition()) {
    size_t cursor_position = GetCursorPosition();
    size_t next_grapheme_index = render_text_->IndexOfAdjacentGrapheme(
        cursor_position, gfx::CURSOR_FORWARD);
    gfx::Range range_to_delete(cursor_position, next_grapheme_index);
    if (add_to_kill_buffer)
      SetKillBuffer(GetTextFromRange(range_to_delete));
    ExecuteAndRecordDelete(range_to_delete, true);
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
  size_t cursor_position = GetCursorPosition();
  if (cursor_position > 0) {
    gfx::Range range_to_delete(
        PlatformStyle::RangeToDeleteBackwards(text(), cursor_position));
    if (add_to_kill_buffer)
      SetKillBuffer(GetTextFromRange(range_to_delete));
    ExecuteAndRecordDelete(range_to_delete, true);
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

bool TextfieldModel::MoveCursorTo(const gfx::Point& point, bool select) {
  if (HasCompositionText())
    ConfirmCompositionText();
  return render_text_->MoveCursorToPoint(point, select);
}

base::string16 TextfieldModel::GetSelectedText() const {
  return GetTextFromRange(render_text_->selection());
}

void TextfieldModel::SelectRange(const gfx::Range& range) {
  if (HasCompositionText())
    ConfirmCompositionText();
  render_text_->SelectRange(range);
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
  return iter == edit_history_.end() || // at the top.
      ++iter != edit_history_.end();
}

bool TextfieldModel::Undo() {
  if (!CanUndo())
    return false;
  DCHECK(!HasCompositionText());
  if (HasCompositionText())
    CancelCompositionText();

  base::string16 old = text();
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
  base::string16 old = text();
  size_t old_cursor = GetCursorPosition();
  (*current_edit_)->Redo(this);
  return old != text() || old_cursor != GetCursorPosition();
}

bool TextfieldModel::Cut() {
  if (!HasCompositionText() && HasSelection() && !render_text_->obscured()) {
    ui::ScopedClipboardWriter(
        ui::CLIPBOARD_TYPE_COPY_PASTE).WriteText(GetSelectedText());
    // A trick to let undo/redo handle cursor correctly.
    // Undoing CUT moves the cursor to the end of the change rather
    // than beginning, unlike Delete/Backspace.
    // TODO(oshima): Change Delete/Backspace to use DeleteSelection,
    // update DeleteEdit and remove this trick.
    const gfx::Range& selection = render_text_->selection();
    render_text_->SelectRange(gfx::Range(selection.end(), selection.start()));
    DeleteSelection();
    return true;
  }
  return false;
}

bool TextfieldModel::Copy() {
  if (!HasCompositionText() && HasSelection() && !render_text_->obscured()) {
    ui::ScopedClipboardWriter(
        ui::CLIPBOARD_TYPE_COPY_PASTE).WriteText(GetSelectedText());
    return true;
  }
  return false;
}

bool TextfieldModel::Paste() {
  base::string16 text;
  ui::Clipboard::GetForCurrentThread()->ReadText(ui::CLIPBOARD_TYPE_COPY_PASTE,
                                                 &text);
  if (text.empty())
    return false;

  base::string16 actual_text = base::CollapseWhitespace(text, false);
  // If the clipboard contains all whitespaces then paste a single space.
  if (actual_text.empty())
    actual_text = base::ASCIIToUTF16(" ");

  InsertTextInternal(actual_text, false);
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
  base::string16 text = GetSelectedText();
  base::string16 transposed_text =
      text.substr(cur - prev) + text.substr(0, cur - prev);

  InsertTextInternal(transposed_text, false);
  return true;
}

bool TextfieldModel::Yank() {
  const base::string16* kill_buffer = GetKillBuffer();
  if (!kill_buffer->empty() || HasSelection()) {
    InsertTextInternal(*kill_buffer, false);
    return true;
  }
  return false;
}

bool TextfieldModel::HasSelection() const {
  return !render_text_->selection().is_empty();
}

void TextfieldModel::DeleteSelection() {
  DCHECK(!HasCompositionText());
  DCHECK(HasSelection());
  ExecuteAndRecordDelete(render_text_->selection(), false);
}

void TextfieldModel::DeleteSelectionAndInsertTextAt(
    const base::string16& new_text,
    size_t position) {
  if (HasCompositionText())
    CancelCompositionText();
  ExecuteAndRecordReplace(DO_NOT_MERGE, render_text_->selection(),
                          position + new_text.length(), new_text, position);
}

base::string16 TextfieldModel::GetTextFromRange(const gfx::Range& range) const {
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
  base::string16 new_text = text();
  render_text_->SetText(new_text.insert(cursor, composition.text));
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

void TextfieldModel::ConfirmCompositionText() {
  DCHECK(HasCompositionText());
  base::string16 composition = text().substr(
      composition_range_.start(), composition_range_.length());
  // TODO(oshima): current behavior on ChromeOS is a bit weird and not
  // sure exactly how this should work. Find out and fix if necessary.
  AddOrMergeEditHistory(std::make_unique<InsertEdit>(
      false, composition, composition_range_.start()));
  render_text_->SetCursorPosition(composition_range_.end());
  ClearComposition();
  if (delegate_)
    delegate_->OnCompositionTextConfirmedOrCleared();
}

void TextfieldModel::CancelCompositionText() {
  DCHECK(HasCompositionText());
  gfx::Range range = composition_range_;
  ClearComposition();
  base::string16 new_text = text();
  render_text_->SetText(new_text.erase(range.start(), range.length()));
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

void TextfieldModel::InsertTextInternal(const base::string16& new_text,
                                        bool mergeable) {
  if (HasCompositionText()) {
    CancelCompositionText();
    ExecuteAndRecordInsert(new_text, mergeable);
  } else if (HasSelection()) {
    ExecuteAndRecordReplaceSelection(mergeable ? MERGEABLE : DO_NOT_MERGE,
                                     new_text);
  } else {
    ExecuteAndRecordInsert(new_text, mergeable);
  }
}

void TextfieldModel::ReplaceTextInternal(const base::string16& new_text,
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

void TextfieldModel::ExecuteAndRecordDelete(gfx::Range range, bool mergeable) {
  size_t old_text_start = range.GetMin();
  const base::string16 old_text = text().substr(old_text_start, range.length());
  bool backward = range.is_reversed();
  gfx::Range curr_selection = render_text_->selection();
  auto edit = std::make_unique<DeleteEdit>(mergeable, old_text, old_text_start,
                                           backward, curr_selection);
  edit->Redo(this);
  AddOrMergeEditHistory(std::move(edit));
}

void TextfieldModel::ExecuteAndRecordReplaceSelection(
    MergeType merge_type,
    const base::string16& new_text) {
  size_t new_text_start = render_text_->selection().GetMin();
  size_t new_cursor_pos = new_text_start + new_text.length();
  ExecuteAndRecordReplace(merge_type, render_text_->selection(), new_cursor_pos,
                          new_text, new_text_start);
}

void TextfieldModel::ExecuteAndRecordReplace(MergeType merge_type,
                                             gfx::Range replacement_range,
                                             size_t new_cursor_pos,
                                             const base::string16& new_text,
                                             size_t new_text_start) {
  size_t old_text_start = replacement_range.GetMin();
  bool backward = replacement_range.is_reversed();
  auto edit = std::make_unique<ReplaceEdit>(
      merge_type, GetTextFromRange(replacement_range), old_text_start,
      render_text_->selection(), backward, new_cursor_pos, new_text,
      new_text_start);
  edit->Redo(this);
  AddOrMergeEditHistory(std::move(edit));
}

void TextfieldModel::ExecuteAndRecordInsert(const base::string16& new_text,
                                            bool mergeable) {
  auto edit =
      std::make_unique<InsertEdit>(mergeable, new_text, GetCursorPosition());
  edit->Redo(this);
  AddOrMergeEditHistory(std::move(edit));
}

void TextfieldModel::AddOrMergeEditHistory(std::unique_ptr<Edit> edit) {
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

void TextfieldModel::ModifyText(size_t delete_from,
                                size_t delete_to,
                                const base::string16& new_text,
                                size_t new_text_insert_at,
                                gfx::Range selection) {
  DCHECK_LE(delete_from, delete_to);
  base::string16 old_text = text();
  ClearComposition();
  if (delete_from != delete_to)
    render_text_->SetText(old_text.erase(delete_from, delete_to - delete_from));
  if (!new_text.empty())
    render_text_->SetText(old_text.insert(new_text_insert_at, new_text));
  if (selection.start() == selection.end()) {
    render_text_->SetCursorPosition(selection.start());
  } else {
    render_text_->SelectRange(selection);
  }
}

// static
void TextfieldModel::ClearKillBuffer() {
  SetKillBuffer(base::string16());
}

}  // namespace views
