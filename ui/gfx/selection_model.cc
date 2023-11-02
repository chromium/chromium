// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/selection_model.h"

#include <ostream>

#include "base/check.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"

namespace gfx {

SelectionModel::SelectionModel()
    : selection_(0),
      caret_affinity_(CURSOR_BACKWARD) {}

SelectionModel::SelectionModel(size_t position, LogicalCursorDirection affinity)
    : selection_(position),
      caret_affinity_(affinity) {}

SelectionModel::SelectionModel(const Range& selection,
                               LogicalCursorDirection affinity)
    : selection_(selection),
      caret_affinity_(affinity) {}

SelectionModel::SelectionModel(const std::vector<Range>& selections,
                               LogicalCursorDirection affinity)
    : selection_(selections[0]), caret_affinity_(affinity) {
  for (size_t i = 1; i < selections.size(); ++i)
    AddSecondarySelection(selections[i]);
}

SelectionModel::SelectionModel(const SelectionModel& selection_model) = default;

SelectionModel::~SelectionModel() = default;

void SelectionModel::AddSecondarySelection(const Range& selection) {
  for (auto s : GetAllSelections())
    DCHECK(!selection.Intersects(s));
  secondary_selections_.push_back(selection);
}

std::vector<Range> SelectionModel::GetAllSelections() const {
  std::vector<Range> selections = {selection()};
  selections.insert(selections.end(), secondary_selections_.begin(),
                    secondary_selections_.end());
  return selections;
}

bool SelectionModel::operator==(const SelectionModel& sel) const {
  return selection() == sel.selection() &&
         caret_affinity() == sel.caret_affinity() &&
         secondary_selections() == sel.secondary_selections();
}

std::string SelectionModel::ToString() const {
  std::string str = "{";
  if (selection().is_empty())
    base::StringAppendF(&str, "%" PRIuS, caret_pos());
  else
    str += selection().ToString();
  const bool backward = caret_affinity() == CURSOR_BACKWARD;
  str += (backward ? ",BACKWARD" : ",FORWARD");
  for (auto selection : secondary_selections()) {
    str += ",";
    if (selection.is_empty())
      base::StringAppendF(&str, "%" PRIuS, selection.end());
    else
      str += selection.ToString();
  }
  return str + "}";
}

std::ostream& operator<<(std::ostream& out, const SelectionModel& model) {
  out << model.ToString();
  return out;
}

}  // namespace gfx
