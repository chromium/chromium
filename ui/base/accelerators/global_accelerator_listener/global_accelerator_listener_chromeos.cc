// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener_chromeos.h"

#include "ui/base/accelerators/accelerator.h"

namespace {
ui::GlobalAcceleratorListenerChromeOS::Delegate* g_delegate = nullptr;
}  // namespace

namespace ui {

// static
std::unique_ptr<GlobalAcceleratorListener>
GlobalAcceleratorListenerChromeOS::Create() {
  return std::make_unique<GlobalAcceleratorListenerChromeOS>(
      base::PassKey<GlobalAcceleratorListenerChromeOS>());
}

// static
void GlobalAcceleratorListenerChromeOS::SetDelegate(Delegate* delegate) {
  g_delegate = delegate;
}

GlobalAcceleratorListenerChromeOS::GlobalAcceleratorListenerChromeOS(
    base::PassKey<GlobalAcceleratorListenerChromeOS>) {}

GlobalAcceleratorListenerChromeOS::~GlobalAcceleratorListenerChromeOS() {
  if (g_delegate) {
    g_delegate->UnregisterAll(this);
  }
}

void GlobalAcceleratorListenerChromeOS::StartListening() {}

void GlobalAcceleratorListenerChromeOS::StopListening() {}

bool GlobalAcceleratorListenerChromeOS::StartListeningForAccelerator(
    const ui::Accelerator& accelerator) {
  CHECK(g_delegate);

  if (accelerator.IsEmpty()) {
    return false;
  }

  if (g_delegate->IsReserved(accelerator) &&
      g_delegate->IsRegistered(accelerator)) {
    return false;
  }

  g_delegate->Register({accelerator}, this);
  return true;
}

void GlobalAcceleratorListenerChromeOS::StopListeningForAccelerator(
    const ui::Accelerator& accelerator) {
  CHECK(g_delegate);
  g_delegate->Unregister(accelerator, this);
}

bool GlobalAcceleratorListenerChromeOS::CanHandleAccelerators() const {
  return true;
}

bool GlobalAcceleratorListenerChromeOS::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  NotifyKeyPressed(accelerator);
  return true;
}

}  // namespace ui
