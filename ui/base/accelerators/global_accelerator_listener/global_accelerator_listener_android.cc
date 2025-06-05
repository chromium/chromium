// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener_android.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/accelerators/accelerator.h"

using content::BrowserThread;

namespace ui {

// static
GlobalAcceleratorListener* GlobalAcceleratorListener::GetInstance() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  static const base::NoDestructor<std::unique_ptr<GlobalAcceleratorListener>>
      instance(GlobalAcceleratorListenerAndroid::Create());
  return instance->get();
}

// static
std::unique_ptr<GlobalAcceleratorListener>
GlobalAcceleratorListenerAndroid::Create() {
  return std::make_unique<GlobalAcceleratorListenerAndroid>(
      base::PassKey<GlobalAcceleratorListenerAndroid>());
}

GlobalAcceleratorListenerAndroid::GlobalAcceleratorListenerAndroid(
    base::PassKey<GlobalAcceleratorListenerAndroid>) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

GlobalAcceleratorListenerAndroid::~GlobalAcceleratorListenerAndroid() = default;

void GlobalAcceleratorListenerAndroid::StartListening() {}

void GlobalAcceleratorListenerAndroid::StopListening() {}

bool GlobalAcceleratorListenerAndroid::StartListeningForAccelerator(
    const ui::Accelerator& accelerator) {
  return false;
}

void GlobalAcceleratorListenerAndroid::StopListeningForAccelerator(
    const ui::Accelerator& accelerator) {}

}  // namespace ui
