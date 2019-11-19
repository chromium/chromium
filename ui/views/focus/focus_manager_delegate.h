// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_FOCUS_FOCUS_MANAGER_DELEGATE_H_
#define UI_VIEWS_FOCUS_FOCUS_MANAGER_DELEGATE_H_

#include "ui/views/views_export.h"

namespace ui {
class Accelerator;
}

namespace views {

class View;

// Delegate interface for views::FocusManager.
class VIEWS_EXPORT FocusManagerDelegate {
 public:
  virtual ~FocusManagerDelegate() = default;

  // Activate the target associated with the specified accelerator.
  // First, AcceleratorPressed handler of the most recently registered target
  // is called, and if that handler processes the event (i.e. returns true),
  // this method immediately returns. If not, we do the same thing on the next
  // target, and so on.
  // Returns true if an accelerator was activated.
  virtual bool ProcessAccelerator(const ui::Accelerator& accelerator) = 0;

  // Called after focus state has changed.
  virtual void OnDidChangeFocus(View* focused_before, View* focused_now) = 0;
};

}  // namespace views

#endif  // UI_VIEWS_FOCUS_FOCUS_MANAGER_DELEGATE_H_
