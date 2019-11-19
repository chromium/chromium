// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_COMBOBOX_COMBOBOX_LISTENER_H_
#define UI_VIEWS_CONTROLS_COMBOBOX_COMBOBOX_LISTENER_H_

#include "ui/views/views_export.h"

namespace views {

class Combobox;

// Interface used to notify consumers when something interesting happens to a
// Combobox.
class VIEWS_EXPORT ComboboxListener {
 public:
  // Invoked when the user does the appropriate gesture that some action should
  // be performed. This is invoked if the user clicks on the menu button and
  // then clicks an item, and also when the menu is not showing and the does a
  // gesture to change the selection (for example, presses the home or end
  // keys). This is not invoked when the menu is shown and the user changes the
  // selection without closing the menu.
  virtual void OnPerformAction(Combobox* combobox) = 0;

 protected:
  virtual ~ComboboxListener() = default;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_COMBOBOX_COMBOBOX_LISTENER_H_
