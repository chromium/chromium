// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_ACCELERATOR_DELEGATE_H_
#define UI_WM_CORE_ACCELERATOR_DELEGATE_H_

namespace ui {
class Accelerator;
class KeyEvent;
}

namespace wm {

class AcceleratorDelegate {
 public:
  virtual ~AcceleratorDelegate() {}

  // Return true if the |accelerator| has been processed.
  virtual bool ProcessAccelerator(const ui::KeyEvent& event,
                                  const ui::Accelerator& accelerator) = 0;
};

}  // namespace wm

#endif  // UI_WM_CORE_ACCELERATOR_DELEGATE_H_
