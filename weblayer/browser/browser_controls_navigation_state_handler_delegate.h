// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BROWSER_CONTROLS_NAVIGATION_STATE_HANDLER_DELEGATE_H_
#define WEBLAYER_BROWSER_BROWSER_CONTROLS_NAVIGATION_STATE_HANDLER_DELEGATE_H_

#include "content/public/common/browser_controls_state.h"

namespace weblayer {

enum class ControlsVisibilityReason;

// Called to propagate changse to BrowserControlsState.
class BrowserControlsNavigationStateHandlerDelegate {
 public:
  // Called when the state changes.
  virtual void OnBrowserControlsStateStateChanged(
      ControlsVisibilityReason reason,
      content::BrowserControlsState state) = 0;

  // Called when UpdateBrowserControlsState() should be called because a new
  // navigation started. This is necessary as the browser-controls state is
  // specific to a renderer, and each navigation may trigger a new renderer.
  virtual void OnUpdateBrowserControlsStateBecauseOfProcessSwitch(
      bool did_commit) = 0;

 protected:
  virtual ~BrowserControlsNavigationStateHandlerDelegate() = default;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BROWSER_CONTROLS_NAVIGATION_STATE_HANDLER_DELEGATE_H_
