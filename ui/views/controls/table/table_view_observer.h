// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TABLE_TABLE_VIEW_OBSERVER_H_
#define UI_VIEWS_CONTROLS_TABLE_TABLE_VIEW_OBSERVER_H_

#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/views_export.h"

namespace views {

// TableViewObserver is notified about the TableView selection.
class VIEWS_EXPORT TableViewObserver {
 public:
  virtual ~TableViewObserver() = default;

  // Invoked when the selection changes.
  virtual void OnSelectionChanged() = 0;

  // Optional method invoked when the user double clicks on the table.
  virtual void OnDoubleClick() {}

  // Optional method invoked when the user middle clicks on the table.
  virtual void OnMiddleClick() {}

  // Optional method invoked when the user hits a key with the table in focus.
  virtual void OnKeyDown(ui::KeyboardCode virtual_keycode) {}
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TABLE_TABLE_VIEW_OBSERVER_H_
