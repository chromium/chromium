// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_model.h"

#include <stddef.h>

#include "base/check_op.h"
#include "base/ranges/algorithm.h"
#include "ui/views/view.h"

namespace views {

// views in `entries_` are owned by their parents, no need to delete them.
ViewModelBase::~ViewModelBase() = default;

void ViewModelBase::Remove(size_t index) {
  check_index(index);
  entries_.erase(entries_.begin() + static_cast<ptrdiff_t>(index));
}

void ViewModelBase::Move(size_t index, size_t target_index) {
  check_index(index);
  check_index(target_index);

  if (index == target_index)
    return;
  Entry entry(entries_[index]);
  entries_.erase(entries_.begin() + static_cast<ptrdiff_t>(index));
  entries_.insert(entries_.begin() + static_cast<ptrdiff_t>(target_index),
                  entry);
}

void ViewModelBase::MoveViewOnly(size_t index, size_t target_index) {
  if (target_index < index) {
    View* view = entries_[index].view;
    for (size_t i = index; i > target_index; --i)
      entries_[i].view = entries_[i - 1].view;
    entries_[target_index].view = view;
  } else if (target_index > index) {
    View* view = entries_[index].view;
    for (size_t i = index; i < target_index; ++i)
      entries_[i].view = entries_[i + 1].view;
    entries_[target_index].view = view;
  }
}

void ViewModelBase::Clear() {
  Entries entries;
  entries.swap(entries_);
  for (const auto& entry : entries)
    delete entry.view;
}

std::optional<size_t> ViewModelBase::GetIndexOfView(const View* view) const {
  const auto i = base::ranges::find(entries_, view, &Entry::view);
  return (i == entries_.cend())
             ? std::nullopt
             : std::make_optional(static_cast<size_t>(i - entries_.cbegin()));
}

ViewModelBase::ViewModelBase() = default;

void ViewModelBase::AddUnsafe(View* view, size_t index) {
  DCHECK_LE(index, entries_.size());
  Entry entry;
  entry.view = view;
  entries_.insert(entries_.begin() + static_cast<ptrdiff_t>(index), entry);
}

}  // namespace views
