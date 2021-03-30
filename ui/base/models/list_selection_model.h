// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_LIST_SELECTION_MODEL_H_
#define UI_BASE_MODELS_LIST_SELECTION_MODEL_H_

#include <stddef.h>

#include "base/component_export.h"
#include "base/containers/flat_set.h"

namespace ui {

// Selection model represented as a list of ints. In addition to the set of
// selected indices ListSelectionModel maintains the following:
//
// active: The index of the currently visible item in the list. This will be
//         kUnselectedIndex if nothing is selected.
//
// anchor: The index of the last item the user clicked on. Extending the
//         selection extends it from this index. This will be kUnselectedIndex
//         if nothing is selected.
//
// Typically there is only one selected item, in which case the anchor and
// active index correspond to the same thing.
class COMPONENT_EXPORT(UI_BASE) ListSelectionModel {
 public:
  using SelectedIndices = base::flat_set<int>;

  // Used to identify no selection.
  static constexpr int kUnselectedIndex = -1;

  ListSelectionModel();
  ListSelectionModel(const ListSelectionModel& other);
  ListSelectionModel(ListSelectionModel&& other) noexcept;

  ~ListSelectionModel();

  ListSelectionModel& operator=(const ListSelectionModel&);
  ListSelectionModel& operator=(ListSelectionModel&&);

  bool operator==(const ListSelectionModel& other) const;
  bool operator!=(const ListSelectionModel& other) const;

  // See class description for details of the anchor.
  void set_anchor(int anchor) { anchor_ = anchor; }
  int anchor() const { return anchor_; }

  // See class description for details of active.
  void set_active(int active) { active_ = active; }
  int active() const { return active_; }

  // True if nothing is selected.
  bool empty() const { return selected_indices_.empty(); }

  // Number of selected indices.
  size_t size() const { return selected_indices_.size(); }

  // Increments all indices >= |index|. For example, if the selection consists
  // of [0, 1, 5] and this is invoked with 1, it results in [0, 2, 6]. This also
  // updates the anchor and active indices.
  // This is used when a new item is inserted into the model.
  void IncrementFrom(int index);

  // Shifts all indices > |index| down by 1. If |index| is selected, it is
  // removed. For example, if the selection consists of [0, 1, 5] and this is
  // invoked with 1, it results in [0, 4]. This is used when an item is
  // removed.
  void DecrementFrom(int index);

  // Sets the anchor, active and selection to |index|.
  void SetSelectedIndex(int index);

  // Returns true if |index| is selected.
  bool IsSelected(int index) const;

  // Adds |index| to the selection. This does not change the active or anchor
  // indices.
  void AddIndexToSelection(int index);

  // Adds indices between |index_start| and |index_end|, inclusive.
  // This does not change the active or anchor indices.
  void AddIndexRangeToSelection(int index_start, int index_end);

  // Removes |index| from the selection. This does not change the active or
  // anchor indices.
  void RemoveIndexFromSelection(int index);

  // Extends the selection from the anchor to |index|. If the anchor is empty,
  // this sets the anchor, selection and active indices to |index|.
  void SetSelectionFromAnchorTo(int index);

  // Makes sure the indices from the anchor to |index| are selected. This only
  // adds to the selection.
  void AddSelectionFromAnchorTo(int index);

  // Invoked when an item moves. |old_index| is the original index, |new_index|
  // is the target index, and |length| is the number of items that are moving.
  //
  // If moving to a greater index, |new_index| should be the index *after*
  // removing the elements at the index range [old_index, old_index + length).
  // For example, consider three list items 'A B C', to move A to the end of
  // the list, this should be invoked with '0, 2, 1'.
  void Move(int old_index, int new_index, int length);

  // Sets the anchor and active to kUnselectedIndex, and removes all the
  // selected indices.
  void Clear();

  // Returns the selected indices. The selection is always ordered in acending
  // order.
  const SelectedIndices& selected_indices() const { return selected_indices_; }

 private:
  SelectedIndices selected_indices_;

  int active_ = kUnselectedIndex;

  int anchor_ = kUnselectedIndex;
};

}  // namespace ui

#endif  // UI_BASE_MODELS_LIST_SELECTION_MODEL_H_
