// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_ENV_OBSERVER_H_
#define UI_AURA_ENV_OBSERVER_H_

#include "ui/aura/aura_export.h"

namespace aura {

class Window;
class WindowTreeHost;

class AURA_EXPORT EnvObserver {
 public:
  // Called when |window| has been initialized.
  virtual void OnWindowInitialized(Window* window) {}

  // Called when a WindowTreeHost is initialized.
  virtual void OnHostInitialized(WindowTreeHost* host) {}

  // Called right before Env is destroyed.
  virtual void OnWillDestroyEnv() {}

  // Called when occlusion tracker pauses/resumes. This is only called in
  // Mode::LOCAL.
  virtual void OnWindowOcclusionTrackingPaused() {}
  virtual void OnWindowOcclusionTrackingResumed() {}

 protected:
  virtual ~EnvObserver() {}
};

}  // namespace aura

#endif  // UI_AURA_ENV_OBSERVER_H_
