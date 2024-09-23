// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_LIST_SELECTION_MODEL_H_
#define UI_BASE_MODELS_LIST_SELECTION_MODEL_H_

#include <stddef.h>

#include <optional>

#include "base/component_export.h"
#include "base/containers/flat_set.h"

namespace ui {

// Selection model represented as a list of indexes. In addition to the set of
// selected indices ListSelectionModel maintains the following:
//
// active: The index of the currently visible item in the list. This will be
//         nullopt if nothing is selected.
//
// anchor: The index of the last item the user clicked on. Extending the
//         selection extends it from this index. This will be nullopt if nothing
//         is selected.
//
// Typically there is only one selected item, in which case the anchor and
// active index correspond to the same thing.
class COMPONENT_EXPORT(UI_BASE) ListSelectionModel {
 public:
  using SelectedIndices = base::flat_set<size_t>;

  ListSelectionModel();
  ListSelectionModel(const ListSelectionModel& other);
  ListSelectionModel(ListSelectionModel&& other) noexcept;

  ~ListSelectionModel();

  ListSelectionModel& operator=(const ListSelectionModel&);
  ListSelectionModel& operator=(ListSelectionModel&&);

  bool operator==(const ListSelectionModel& other) const;
  bool operator!=(const ListSelectionModel& other) const;

  // See class description for details of the anchor.
  void set_anchor(std::optional<size_t> anchor) { anchor_ = anchor; }
  std::optional<size_t> anchor() const { return anchor_; }

  // See class description for details of active.
  void set_active(std::optional<size_t> active) { active_ = active; }
  std::optional<size_t> active() const { return active_; }

  // True if nothing is selected.
  bool empty() const { return selected_indices_.empty(); }

  // Number of selected indices.
  size_t size() const { return selected_indices_.size(); }

  // Increments all indices >= |index|. For example, if the selection consists
  // of [0, 1, 5] and this is invoked with 1, it results in [0, 2, 6]. This also
  // updates the anchor and active indices.
  // This is used when a new item is inserted into the model.
  void IncrementFrom(size_t index);

  // Shifts all indices > |index| down by 1. If |index| is selected, it is
  // removed. For example, if the selection consists of [0, 1, 5] and this is
  // invoked with 1, it results in [0, 4]. This is used when an item is
  // removed.
  void DecrementFrom(size_t index);

  // Sets the anchor, active and selection to |index|.
  void SetSelectedIndex(std::optional<size_t> index);

  // Returns true if |index| is selected.
  bool IsSelected(size_t index) const;

  // Adds |index| to the selection. This does not change the active or anchor
  // indices.
  void AddIndexToSelection(size_t index);

  // Adds indices between |index_start| and |index_end|, inclusive.
  // This does not change the active or anchor indices.
  void AddIndexRangeToSelection(size_t index_start, size_t index_end);

  // Removes |index| from the selection. This does not change the active or
  // anchor indices.
  void RemoveIndexFromSelection(size_t index);

  // Extends the selection from the anchor to |index|. If the anchor is empty,
  // this sets the anchor, selection and active indices to |index|.
  void SetSelectionFromAnchorTo(size_t index);

  // Makes sure the indices from the anchor to |index| are selected. This only
  // adds to the selection.
  void AddSelectionFromAnchorTo(size_t index);

  // Invoked when an item moves. |old_index| is the original index, |new_index|
  // is the target index, and |length| is the number of items that are moving.
  //
  // If moving to a greater index, |new_index| should be the index *after*
  // removing the elements at the index range [old_index, old_index + length).
  // For example, consider three list items 'A B C', to move A to the end of
  // the list, this should be invoked with '0, 2, 1'.
  void Move(size_t old_index, size_t new_index, size_t length);

  // Sets the anchor and active to kUnselectedIndex, and removes all the
  // selected indices.
  void Clear();

  // Returns the selected indices. The selection is always ordered in acending
  // order.
  const SelectedIndices& selected_indices() const { return selected_indices_; }

  // Returns the state of the selection model as a string. The format is:
  // 'active=X anchor=X selection=X X X...'.
  std::string ToString() const;

 private:
  SelectedIndices selected_indices_;
  std::optional<size_t> active_ = std::nullopt;
  std::optional<size_t> anchor_ = std::nullopt;
};

}  // namespace ui

#endif  // UI_BASE_MODELS_LIST_SELECTION_MODEL_H_
