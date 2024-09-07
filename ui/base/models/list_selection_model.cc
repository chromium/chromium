// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/list_selection_model.h"

#include <algorithm>
#include <valarray>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

namespace ui {

namespace {

void IncrementFromImpl(size_t index, size_t* value) {
  if (*value >= index)
    (*value)++;
}

void IncrementFromImpl(size_t index, std::optional<size_t>* value) {
  if (value->has_value())
    IncrementFromImpl(index, &value->value());
}

// Returns true if |value| should be erased from its container.
bool DecrementFromImpl(size_t index, size_t* value) {
  if (*value == index)
    return true;
  if (*value > index)
    (*value)--;
  return false;
}

void DecrementFromImpl(size_t index, std::optional<size_t>* value) {
  if (value->has_value() && DecrementFromImpl(index, &value->value()))
    *value = std::nullopt;
}

void MoveToLowerIndexImpl(size_t old_start,
                          size_t new_start,
                          size_t length,
                          size_t* value) {
  DCHECK_LE(new_start, old_start);
  // When a range of items moves to a lower index, the only affected indices
  // are those in the interval [new_start, old_start + length).
  if (new_start <= *value && *value < old_start + length) {
    if (*value < old_start) {
      // The items originally in the interval [new_start, old_start) see
      // |length| many items inserted before them, so their indices increase.
      *value += length;
    } else {
      // The items originally in the interval [old_start, old_start + length)
      // are shifted downward by (old_start - new_start) many spots, so
      // their indices decrease.
      *value -= (old_start - new_start);
    }
  }
}

}  // namespace

ListSelectionModel::ListSelectionModel() = default;
ListSelectionModel::ListSelectionModel(const ListSelectionModel&) = default;
ListSelectionModel::ListSelectionModel(ListSelectionModel&&) noexcept = default;

ListSelectionModel::~ListSelectionModel() = default;

ListSelectionModel& ListSelectionModel::operator=(const ListSelectionModel&) =
    default;
ListSelectionModel& ListSelectionModel::operator=(ListSelectionModel&&) =
    default;

bool ListSelectionModel::operator==(const ListSelectionModel& other) const {
  return std::tie(active_, anchor_, selected_indices_) ==
         std::tie(other.active_, other.anchor_, other.selected_indices_);
}

bool ListSelectionModel::operator!=(const ListSelectionModel& other) const {
  return !operator==(other);
}

void ListSelectionModel::IncrementFrom(size_t index) {
  // Shift the selection to account for a newly inserted item at |index|.
  for (size_t& selected_index : selected_indices_)
    IncrementFromImpl(index, &selected_index);
  IncrementFromImpl(index, &anchor_);
  IncrementFromImpl(index, &active_);
}

void ListSelectionModel::DecrementFrom(size_t index) {
  for (auto i = selected_indices_.begin(); i != selected_indices_.end();) {
    if (DecrementFromImpl(index, &(*i))) {
      i = selected_indices_.erase(i);
    } else {
      ++i;
    }
  }
  DecrementFromImpl(index, &anchor_);
  DecrementFromImpl(index, &active_);
}

void ListSelectionModel::SetSelectedIndex(std::optional<size_t> index) {
  anchor_ = active_ = index;
  selected_indices_.clear();
  if (index.has_value()) {
    selected_indices_.insert(index.value());
  }
}

bool ListSelectionModel::IsSelected(size_t index) const {
  return base::Contains(selected_indices_, index);
}

void ListSelectionModel::AddIndexToSelection(size_t index) {
  selected_indices_.insert(index);
}

void ListSelectionModel::AddIndexRangeToSelection(size_t index_start,
                                                  size_t index_end) {
  DCHECK_LE(index_start, index_end);

  if (index_start == index_end)
    return AddIndexToSelection(index_start);

  for (size_t i = index_start; i <= index_end; ++i) {
    selected_indices_.insert(i);
  }
}

void ListSelectionModel::RemoveIndexFromSelection(size_t index) {
  selected_indices_.erase(index);
}

void ListSelectionModel::SetSelectionFromAnchorTo(size_t index) {
  if (!anchor_.has_value()) {
    SetSelectedIndex(index);
  } else {
    SelectedIndices new_selection;
    for (size_t min = std::min(index, anchor_.value()),
                delta = std::max(index, anchor_.value()) - min, i = min;
         i <= min + delta; ++i) {
      new_selection.insert(i);
    }
    selected_indices_.swap(new_selection);
    active_ = index;
  }
}

void ListSelectionModel::AddSelectionFromAnchorTo(size_t index) {
  if (!anchor_.has_value()) {
    SetSelectedIndex(index);
  } else {
    for (size_t i = std::min(index, anchor_.value()),
                end = std::max(index, anchor_.value());
         i <= end; ++i) {
      selected_indices_.insert(i);
    }
    active_ = index;
  }
}

void ListSelectionModel::Move(size_t old_index,
                              size_t new_index,
                              size_t length) {
  // |length| many items are moving from index |old_index| to index |new_index|.
  DCHECK_NE(old_index, new_index);
  DCHECK_GT(length, 0u);

  // Remap move-to-higher-index operations to the equivalent move-to-lower-index
  // operation. As an example, the permutation "ABCDEFG" -> "CDEFABG" can be
  // thought of either as shifting 'AB' higher by 4, or by shifting 'CDEF' lower
  // by 2.
  if (new_index > old_index) {
    Move(old_index + length, old_index, new_index - old_index);
    return;
  }

  // We know that |old_index| > |new_index|, so this is a move to a lower index.
  // Start by transforming |anchor_| and |active_|.
  if (anchor_.has_value())
    MoveToLowerIndexImpl(old_index, new_index, length, &anchor_.value());
  if (active_.has_value())
    MoveToLowerIndexImpl(old_index, new_index, length, &active_.value());

  // When a range of items moves to a lower index, the affected items are those
  // in the interval [new_index, old_index + length). Search within
  // |selected_indices_| for indices that fall into that range.
  auto low = std::lower_bound(selected_indices_.begin(),
                              selected_indices_.end(), new_index);
  auto high =
      std::lower_bound(low, selected_indices_.end(), old_index + length);

  // The items originally in the interval [new_index, old_index) will see
  // |length| many items inserted before them, so their indices increase.
  auto middle = std::lower_bound(low, high, old_index);
  size_t pivot_value = new_index + length;
  for (auto it = low; it != middle; ++it) {
    (*it) += length;
    DCHECK_LE(pivot_value, *it);
    DCHECK_LT(*it, old_index + length);
  }

  // The items originally in the interval [old_index, old_index + length) are
  // shifted downward by (old_index - new_index) many spots, so their indices
  // decrease.
  for (auto it = middle; it != high; ++it) {
    (*it) -= (old_index - new_index);
    DCHECK_LE(new_index, *it);
    DCHECK_LT(*it, pivot_value);
  }

  // Reorder the ranges [low, middle), and [middle, high) so that the elements
  // in [middle, high) appear first, followed by [low, middle). This suffices to
  // restore the sort condition on |selected_indices_|, because each range is
  // still sorted piecewise, and |pivot_value| is a lower bound for elements in
  // [low, middle), and an upper bound for [middle, high).
  std::rotate(low, middle, high);
}

void ListSelectionModel::Clear() {
  anchor_ = active_ = std::nullopt;
  selected_indices_.clear();
}

std::string ListSelectionModel::ToString() const {
  const auto optional_to_string = [](const auto& opt) {
    return opt.has_value() ? base::NumberToString(opt.value())
                           : std::string("<none>");
  };
  std::vector<std::string> index_strings;
  base::ranges::transform(
      selected_indices_, std::back_inserter(index_strings),
      [](const auto& index) { return base::NumberToString(index); });
  return "active=" + optional_to_string(active_) +
         " anchor=" + optional_to_string(anchor_) +
         " selection=" + base::JoinString(index_strings, " ");
}

}  // namespace ui
