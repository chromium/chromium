// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_PREFIX_DELEGATE_H_
#define UI_VIEWS_CONTROLS_PREFIX_DELEGATE_H_

#include <optional>

#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace views {

// An interface used to expose lists of items for selection by text input.
class VIEWS_EXPORT PrefixDelegate {
 public:
  // Returns the total number of selectable items.
  virtual size_t GetRowCount() = 0;

  // Returns the row of the currently selected item, or -1 if no item is
  // selected.
  virtual std::optional<size_t> GetSelectedRow() = 0;

  // Sets the selection to the specified row.
  virtual void SetSelectedRow(std::optional<size_t> row) = 0;

  // Returns the item at the specified row.
  virtual std::u16string GetTextForRow(size_t row) = 0;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_PREFIX_DELEGATE_H_
