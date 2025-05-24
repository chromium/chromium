// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_GLOBAL_ACCELERATOR_LISTENER_GLOBAL_ACCELERATOR_LISTENER_OZONE_H_
#define UI_BASE_ACCELERATORS_GLOBAL_ACCELERATOR_LISTENER_GLOBAL_ACCELERATOR_LISTENER_OZONE_H_

#include <memory>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/ozone/public/platform_global_shortcut_listener.h"

namespace ui {
class Accelerator;
}  // namespace ui

namespace ui {

// Ozone-specific implementation of the GlobalAcceleratorListener interface.
//
// Connects Aura with the platform implementation, and manages data conversions
// required on the way: Aura operates with ui::Accelerator while the platform is
// only aware of the basic components such as the key code and modifiers.
class GlobalAcceleratorListenerOzone
    : public GlobalAcceleratorListener,
      public ui::PlatformGlobalShortcutListenerDelegate {
 public:
  static std::unique_ptr<GlobalAcceleratorListener> Create();

  // Clients should use Create() instead of using this constructor.
  explicit GlobalAcceleratorListenerOzone(
      base::PassKey<GlobalAcceleratorListenerOzone>);

  GlobalAcceleratorListenerOzone(const GlobalAcceleratorListenerOzone&) =
      delete;
  GlobalAcceleratorListenerOzone& operator=(
      const GlobalAcceleratorListenerOzone&) = delete;

  ~GlobalAcceleratorListenerOzone() override;

 private:
  // GlobalAcceleratorListener:
  void StartListening() override;
  void StopListening() override;
  bool StartListeningForAccelerator(
      const ui::Accelerator& accelerator) override;
  void StopListeningForAccelerator(const ui::Accelerator& accelerator) override;

  // ui::PlatformGlobalShortcutListenerDelegate:
  void OnKeyPressed(ui::KeyboardCode key_code,
                    bool is_alt_down,
                    bool is_ctrl_down,
                    bool is_shift_down) override;
  void OnPlatformListenerDestroyed() override;

  bool is_listening_ = false;
  std::set<ui::Accelerator> registered_hot_keys_;

  // The platform implementation.
  raw_ptr<ui::PlatformGlobalShortcutListener>
      platform_global_shortcut_listener_ = nullptr;
};

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_GLOBAL_ACCELERATOR_LISTENER_GLOBAL_ACCELERATOR_LISTENER_OZONE_H_
