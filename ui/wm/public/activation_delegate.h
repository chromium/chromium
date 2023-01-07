// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_PUBLIC_ACTIVATION_DELEGATE_H_
#define UI_WM_PUBLIC_ACTIVATION_DELEGATE_H_

#include "ui/wm/public/wm_public_export.h"

namespace aura {
class Window;
}

namespace wm {

// An interface implemented by an object that configures and responds to changes
// to a window's activation state.
class WM_PUBLIC_EXPORT ActivationDelegate {
 public:
  // Returns true if the window should be activated.
  virtual bool ShouldActivate() const = 0;

 protected:
  virtual ~ActivationDelegate() {}
};

// Sets/Gets the ActivationDelegate on the Window. No ownership changes.
WM_PUBLIC_EXPORT void SetActivationDelegate(aura::Window* window,
                                            ActivationDelegate* delegate);
WM_PUBLIC_EXPORT ActivationDelegate* GetActivationDelegate(
    const aura::Window* window);

}  // namespace wm

#endif  // UI_WM_PUBLIC_ACTIVATION_DELEGATE_H_
