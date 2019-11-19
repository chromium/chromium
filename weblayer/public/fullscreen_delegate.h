// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_FULLSCREEN_DELEGATE_H_
#define WEBLAYER_PUBLIC_FULLSCREEN_DELEGATE_H_

#include "base/callback_forward.h"

namespace weblayer {

class FullscreenDelegate {
 public:
  // Called when the page has requested to go fullscreen.
  virtual void EnterFullscreen(base::OnceClosure exit_closure) = 0;

  // Informs the delegate the page has exited fullscreen.
  virtual void ExitFullscreen() = 0;

 protected:
  virtual ~FullscreenDelegate() {}
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_FULLSCREEN_DELEGATE_H_
