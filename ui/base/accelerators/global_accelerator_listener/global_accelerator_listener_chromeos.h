// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_GLOBAL_ACCELERATOR_LISTENER_GLOBAL_ACCELERATOR_LISTENER_CHROMEOS_H_
#define UI_BASE_ACCELERATORS_GLOBAL_ACCELERATOR_LISTENER_GLOBAL_ACCELERATOR_LISTENER_CHROMEOS_H_

#include <memory>
#include <vector>

#include "base/types/pass_key.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"

namespace ui {

// ChromeOS-specific implementation of the GlobalAcceleratorListener interface.
class GlobalAcceleratorListenerChromeOS : public GlobalAcceleratorListener,
                                          public ui::AcceleratorTarget {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    // Registers global keyboard accelerators for the specified target. If
    // multiple targets are registered for any given accelerator, a target
    // registered later has higher priority.
    virtual void Register(const std::vector<ui::Accelerator>& accelerators,
                          ui::AcceleratorTarget* target) = 0;
    // Unregisters the specified keyboard accelerator for the specified target.
    virtual void Unregister(const ui::Accelerator& accelerator,
                            ui::AcceleratorTarget* target) = 0;
    // Unregisters all keyboard accelerators for the specified target.
    virtual void UnregisterAll(ui::AcceleratorTarget* target) = 0;
    // Returns true if the |accelerator| is reserved.
    virtual bool IsReserved(const ui::Accelerator& accelerator) const = 0;
    // Returns true if the |accelerator| is registered for any target.
    virtual bool IsRegistered(const ui::Accelerator& accelerator) const = 0;
  };

  static std::unique_ptr<GlobalAcceleratorListener> Create();
  static void SetDelegate(Delegate* delegate);

  explicit GlobalAcceleratorListenerChromeOS(
      base::PassKey<GlobalAcceleratorListenerChromeOS>);

  GlobalAcceleratorListenerChromeOS(const GlobalAcceleratorListenerChromeOS&) =
      delete;
  GlobalAcceleratorListenerChromeOS& operator=(
      const GlobalAcceleratorListenerChromeOS&) = delete;

  ~GlobalAcceleratorListenerChromeOS() override;

 private:
  // GlobalAcceleratorListener:
  void StartListening() override;
  void StopListening() override;
  bool StartListeningForAccelerator(
      const ui::Accelerator& accelerator) override;
  void StopListeningForAccelerator(const ui::Accelerator& accelerator) override;

  // ui::AcceleratorTarget:
  bool CanHandleAccelerators() const override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
};

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_GLOBAL_ACCELERATOR_LISTENER_GLOBAL_ACCELERATOR_LISTENER_CHROMEOS_H_
