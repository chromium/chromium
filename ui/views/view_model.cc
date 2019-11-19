// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_model.h"

#include <stddef.h>

#include "base/logging.h"
#include "ui/views/view.h"

namespace views {

ViewModelBase::~ViewModelBase() {
  // view are owned by their parent, no need to delete them.
}

void ViewModelBase::Remove(int index) {
  if (index == -1)
    return;

  check_index(index);
  entries_.erase(entries_.begin() + index);
}

void ViewModelBase::Move(int index, int target_index) {
  DCHECK_LT(index, static_cast<int>(entries_.size()));
  DCHECK_GE(index, 0);
  DCHECK_LT(target_index, static_cast<int>(entries_.size()));
  DCHECK_GE(target_index, 0);

  if (index == target_index)
    return;
  Entry entry(entries_[index]);
  entries_.erase(entries_.begin() + index);
  entries_.insert(entries_.begin() + target_index, entry);
}

void ViewModelBase::MoveViewOnly(int index, int target_index) {
  if (target_index < index) {
    View* view = entries_[index].view;
    for (int i = index; i > target_index; --i)
      entries_[i].view = entries_[i - 1].view;
    entries_[target_index].view = view;
  } else if (target_index > index) {
    View* view = entries_[index].view;
    for (int i = index; i < target_index; ++i)
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

int ViewModelBase::GetIndexOfView(const View* view) const {
  const auto i =
      std::find_if(entries_.cbegin(), entries_.cend(),
                   [view](const auto& entry) { return entry.view == view; });
  return (i == entries_.cend()) ? -1 : (i - entries_.cbegin());
}

ViewModelBase::ViewModelBase() = default;

void ViewModelBase::AddUnsafe(View* view, int index) {
  DCHECK_LE(index, static_cast<int>(entries_.size()));
  DCHECK_GE(index, 0);
  Entry entry;
  entry.view = view;
  entries_.insert(entries_.begin() + index, entry);
}

}  // namespace views
