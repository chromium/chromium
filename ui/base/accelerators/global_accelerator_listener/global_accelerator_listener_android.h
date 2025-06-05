// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_GLOBAL_ACCELERATOR_LISTENER_GLOBAL_ACCELERATOR_LISTENER_ANDROID_H_
#define UI_BASE_ACCELERATORS_GLOBAL_ACCELERATOR_LISTENER_GLOBAL_ACCELERATOR_LISTENER_ANDROID_H_

#include <memory>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {
class Accelerator;
}  // namespace ui

namespace ui {

// Android-specific implementation of the GlobalAcceleratorListener interface.
//
// Currently it does nothing.
class GlobalAcceleratorListenerAndroid : public GlobalAcceleratorListener {
 public:
  static std::unique_ptr<GlobalAcceleratorListener> Create();

  // Clients should use Create() instead of using this constructor.
  explicit GlobalAcceleratorListenerAndroid(
      base::PassKey<GlobalAcceleratorListenerAndroid>);

  GlobalAcceleratorListenerAndroid(const GlobalAcceleratorListenerAndroid&) =
      delete;
  GlobalAcceleratorListenerAndroid& operator=(
      const GlobalAcceleratorListenerAndroid&) = delete;

  ~GlobalAcceleratorListenerAndroid() override;

 private:
  // GlobalAcceleratorListener:
  void StartListening() override;
  void StopListening() override;
  bool StartListeningForAccelerator(const Accelerator& accelerator) override;
  void StopListeningForAccelerator(const Accelerator& accelerator) override;
};

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_GLOBAL_ACCELERATOR_LISTENER_GLOBAL_ACCELERATOR_LISTENER_ANDROID_H_
