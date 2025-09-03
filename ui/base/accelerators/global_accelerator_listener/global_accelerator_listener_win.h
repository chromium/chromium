// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_GLOBAL_ACCELERATOR_LISTENER_GLOBAL_ACCELERATOR_LISTENER_WIN_H_
#define UI_BASE_ACCELERATORS_GLOBAL_ACCELERATOR_LISTENER_GLOBAL_ACCELERATOR_LISTENER_WIN_H_

#include <windows.h>

#include <memory>

#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"
#include "ui/base/accelerators/media_keys_listener.h"

namespace gfx {

class SingletonHwndHotKeyObserver;

}  // namespace gfx

namespace ui {

// Windows-specific implementation of the GlobalAcceleratorListener class that
// listens for global accelerators. Handles setting up a keyboard hook and
// forwarding its output to the base class for processing.
class GlobalAcceleratorListenerWin : public GlobalAcceleratorListener,
                                     public ui::MediaKeysListener::Delegate {
 public:
  GlobalAcceleratorListenerWin();

  GlobalAcceleratorListenerWin(const GlobalAcceleratorListenerWin&) = delete;
  GlobalAcceleratorListenerWin& operator=(const GlobalAcceleratorListenerWin&) =
      delete;

  ~GlobalAcceleratorListenerWin() override;

 private:
  // The implementation of our Window Proc, called by SingletonHwnd.
  void OnWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  // GlobalAcceleratorListener implementation.
  void StartListening() override;
  void StopListening() override;
  bool StartListeningForAccelerator(
      const ui::Accelerator& accelerator) override;
  void StopListeningForAccelerator(const ui::Accelerator& accelerator) override;

  // ui::MediaKeysListener::Delegate implementation.
  void OnMediaKeysAccelerator(const ui::Accelerator& accelerator) override;

  // Whether this object is listening for global accelerators.
  bool is_listening_;

  // The number of media keys currently registered.
  int registered_media_keys_ = 0;

  // A map of registered accelerators and their registration ids. The value is
  // null for media keys if kHardwareMediaKeyHandling is true.
  using HotKeyMap = std::map<ui::Accelerator,
                             std::unique_ptr<gfx::SingletonHwndHotKeyObserver>>;
  HotKeyMap hotkeys_;
};

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_GLOBAL_ACCELERATOR_LISTENER_GLOBAL_ACCELERATOR_LISTENER_WIN_H_
