// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/accelerator_manager.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util.h"

namespace ui {

AcceleratorManager::AcceleratorManager() = default;

AcceleratorManager::~AcceleratorManager() = default;

void AcceleratorManager::Register(
    const std::vector<ui::Accelerator>& accelerators,
    HandlerPriority priority,
    AcceleratorTarget* target) {
  DCHECK(target);

  for (const ui::Accelerator& accelerator : accelerators) {
    AcceleratorTargetList& targets = accelerators_[accelerator].second;
    DCHECK(!base::Contains(targets, target))
        << "Registering the same target multiple times";

    // All priority accelerators go to the front of the line.
    if (priority == kHighPriority) {
      DCHECK(!accelerators_[accelerator].first)
          << "Only one high-priority handler can be registered";
      targets.push_front(target);
      // Mark that we have a priority accelerator at the front.
      accelerators_[accelerator].first = true;
    } else {
      // We are registering a normal priority handler. If no priority
      // accelerator handler has been registered before us, just add the new
      // handler to the front. Otherwise, register it after the first (only)
      // priority handler.
      if (!accelerators_[accelerator].first)
        targets.push_front(target);
      else
        targets.insert(++targets.begin(), target);
    }
  }
}

void AcceleratorManager::Unregister(const Accelerator& accelerator,
                                    AcceleratorTarget* target) {
  auto map_iter = accelerators_.find(accelerator);
  if (map_iter == accelerators_.end()) {
    NOTREACHED() << "Unregistering non-existing accelerator";
    return;
  }

  UnregisterImpl(map_iter, target);
}

void AcceleratorManager::UnregisterAll(AcceleratorTarget* target) {
  for (auto map_iter = accelerators_.begin();
       map_iter != accelerators_.end();) {
    AcceleratorTargetList* targets = &map_iter->second.second;
    if (!base::Contains(*targets, target)) {
      ++map_iter;
    } else {
      auto tmp_iter = map_iter;
      ++map_iter;
      UnregisterImpl(tmp_iter, target);
    }
  }
}

bool AcceleratorManager::IsRegistered(const Accelerator& accelerator) const {
  auto map_iter = accelerators_.find(accelerator);
  return map_iter != accelerators_.end() && !map_iter->second.second.empty();
}

bool AcceleratorManager::Process(const Accelerator& accelerator) {
  auto map_iter = accelerators_.find(accelerator);
  if (map_iter == accelerators_.end())
    return false;

  // We have to copy the target list here, because an AcceleratorPressed
  // event handler may modify the list.
  AcceleratorTargetList targets(map_iter->second.second);
  for (auto iter = targets.begin(); iter != targets.end(); ++iter) {
    if ((*iter)->CanHandleAccelerators() &&
        (*iter)->AcceleratorPressed(accelerator)) {
      return true;
    }
  }

  return false;
}

bool AcceleratorManager::HasPriorityHandler(
    const Accelerator& accelerator) const {
  auto map_iter = accelerators_.find(accelerator);
  if (map_iter == accelerators_.end() || map_iter->second.second.empty())
    return false;

  // Check if we have a priority handler. If not, there's no more work needed.
  if (!map_iter->second.first)
    return false;

  // If the priority handler says it cannot handle the accelerator, we must not
  // count it as one.
  return map_iter->second.second.front()->CanHandleAccelerators();
}

void AcceleratorManager::UnregisterImpl(AcceleratorMap::iterator map_iter,
                                        AcceleratorTarget* target) {
  AcceleratorTargetList* targets = &map_iter->second.second;
  auto target_iter = std::find(targets->begin(), targets->end(), target);
  if (target_iter == targets->end()) {
    NOTREACHED() << "Unregistering accelerator for wrong target";
    return;
  }

  // Only one priority handler is allowed, so if we remove the first element we
  // no longer have a priority target.
  if (target_iter == targets->begin())
    map_iter->second.first = false;

  targets->remove(target);
  if (!targets->empty())
    return;
  accelerators_.erase(map_iter);
}

}  // namespace ui
