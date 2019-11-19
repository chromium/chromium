// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TABBED_PANE_TABBED_PANE_LISTENER_H_
#define UI_VIEWS_CONTROLS_TABBED_PANE_TABBED_PANE_LISTENER_H_

#include "ui/views/views_export.h"

namespace views {

// An interface implemented by an object to let it know that a tabbed pane was
// selected by the user at the specified index.
class VIEWS_EXPORT TabbedPaneListener {
 public:
  // Called when the tab at |index| is selected by the user.
  virtual void TabSelectedAt(int index) = 0;

 protected:
  virtual ~TabbedPaneListener() = default;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TABBED_PANE_TABBED_PANE_LISTENER_H_
