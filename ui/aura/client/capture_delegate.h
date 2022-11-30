// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_CAPTURE_DELEGATE_H_
#define UI_AURA_CLIENT_CAPTURE_DELEGATE_H_

#include "ui/aura/aura_export.h"

namespace aura {
class Window;
namespace client {

// This interface provides API to change the root Window's capture state without
// exposing them as RootWindow API.
class AURA_EXPORT CaptureDelegate {
 public:
  // Called when a capture is set on |new_capture| and/or a capture is
  // released on |old_capture|.
  // NOTE: |old_capture| and |new_capture| are not necessarily contained in the
  // window hierarchy of the delegate.
  virtual void UpdateCapture(aura::Window* old_capture,
                             aura::Window* new_capture) = 0;

  // Called when another root gets capture.
  virtual void OnOtherRootGotCapture() = 0;

  // Sets/Release a native capture on host windows.
  virtual void SetNativeCapture() = 0;
  virtual void ReleaseNativeCapture() = 0;

 protected:
  virtual ~CaptureDelegate() {}
};

}  // namespace client
}  // namespace aura

#endif  // UI_AURA_CLIENT_CAPTURE_DELEGATE_H_
