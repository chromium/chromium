// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"

#include <vector>

#include "base/check.h"
#include "ui/base/accelerators/accelerator.h"

namespace ui {

GlobalAcceleratorListener::GlobalAcceleratorListener() = default;

GlobalAcceleratorListener::~GlobalAcceleratorListener() {
  DCHECK(accelerator_map_.empty());  // Make sure we've cleaned up.
}

bool GlobalAcceleratorListener::RegisterAccelerator(
    const ui::Accelerator& accelerator,
    Observer* observer) {
  if (IsShortcutHandlingSuspended()) {
    return false;
  }

  AcceleratorMap::const_iterator it = accelerator_map_.find(accelerator);
  if (it != accelerator_map_.end()) {
    // The accelerator has been registered.
    return false;
  }

  if (!StartListeningForAccelerator(accelerator)) {
    // If the platform-specific registration fails, mostly likely the
    // accelerator has been registered by other native applications.
    return false;
  }

  if (accelerator_map_.empty()) {
    StartListening();
  }

  accelerator_map_[accelerator] = observer;
  return true;
}

void GlobalAcceleratorListener::UnregisterAccelerator(
    const ui::Accelerator& accelerator,
    Observer* observer) {
  if (IsShortcutHandlingSuspended()) {
    return;
  }

  auto it = accelerator_map_.find(accelerator);
  // We should never get asked to unregister something that we didn't register.
  CHECK(it != accelerator_map_.end());
  // The caller should call this function with the right observer.
  DCHECK(it->second == observer);

  StopListeningForAccelerator(accelerator);
  accelerator_map_.erase(it);
  if (accelerator_map_.empty()) {
    StopListening();
  }
}

void GlobalAcceleratorListener::UnregisterAccelerators(Observer* observer) {
  if (IsShortcutHandlingSuspended()) {
    return;
  }

  std::vector<ui::Accelerator> removed_accelerators;

  auto it = accelerator_map_.begin();
  while (it != accelerator_map_.end()) {
    if (it->second == observer) {
      auto to_remove = it++;
      removed_accelerators.emplace_back(to_remove->first);
      UnregisterAccelerator(to_remove->first, observer);
    } else {
      ++it;
    }
  }
}

void GlobalAcceleratorListener::SetShortcutHandlingSuspended(bool suspended) {
  if (shortcut_handling_suspended_ == suspended) {
    return;
  }

  shortcut_handling_suspended_ = suspended;
  for (auto& it : accelerator_map_) {
    // On Linux, when shortcut handling is suspended we cannot simply early
    // return in NotifyKeyPressed (similar to what we do for non-global
    // shortcuts) because we'd eat the keyboard event thereby preventing the
    // user from setting the shortcut. Therefore we must unregister while
    // handling is suspended and register when handling resumes.
    if (shortcut_handling_suspended_) {
      StopListeningForAccelerator(it.first);
    } else {
      StartListeningForAccelerator(it.first);
    }
  }
}

bool GlobalAcceleratorListener::IsShortcutHandlingSuspended() const {
  return shortcut_handling_suspended_;
}

bool GlobalAcceleratorListener::IsRegistrationHandledExternally() const {
  return false;
}

void GlobalAcceleratorListener::NotifyKeyPressed(
    const ui::Accelerator& accelerator) {
  auto iter = accelerator_map_.find(accelerator);

  // This should never occur, because if it does, we have failed to unregister
  // or failed to clean up the map after unregistering the accelerator.
  CHECK(iter != accelerator_map_.end());

  iter->second->OnKeyPressed(accelerator);
}

}  // namespace ui
