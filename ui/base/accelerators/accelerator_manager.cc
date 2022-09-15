// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/accelerator_manager.h"

#include <ostream>

#include "base/check.h"
#include "base/containers/contains.h"

namespace ui {

AcceleratorManager::AcceleratorManager() = default;

AcceleratorManager::~AcceleratorManager() = default;

void AcceleratorManager::Register(
    const std::vector<ui::Accelerator>& accelerators,
    HandlerPriority priority,
    AcceleratorTarget* target) {
  DCHECK(target);

  for (const ui::Accelerator& accelerator : accelerators) {
    accelerators_.GetOrInsertDefault(accelerator)
        .RegisterWithPriority(target, priority);
  }
}

void AcceleratorManager::Unregister(const Accelerator& accelerator,
                                    AcceleratorTarget* target) {
  DCHECK(target);
  AcceleratorTargetInfo* target_info = accelerators_.Find(accelerator);
  DCHECK(target_info) << "Unregistering non-existing accelerator";

  const bool was_registered = target_info->Unregister(target);
  DCHECK(was_registered) << "Unregistering accelerator for wrong target";

  // If the last target for the accelerator is removed, then erase the
  // entry from the map.
  if (!target_info->HasTargets())
    accelerators_.Erase(accelerator);
}

void AcceleratorManager::UnregisterAll(AcceleratorTarget* target) {
  for (auto map_iter = accelerators_.begin();
       map_iter != accelerators_.end();) {
    AcceleratorTargetInfo& target_info = map_iter->second;

    // Unregister the target and remove the entry if it was the last target.
    const bool was_registered = target_info.Unregister(target);
    if (was_registered && !target_info.HasTargets()) {
      Accelerator key_to_remove = map_iter->first;
      ++map_iter;
      accelerators_.Erase(key_to_remove);
      continue;
    }

    DCHECK(target_info.HasTargets());
    ++map_iter;
  }
}

bool AcceleratorManager::IsRegistered(const Accelerator& accelerator) const {
  const AcceleratorTargetInfo* target_info = accelerators_.Find(accelerator);

  // If the accelerator is in the map, the target list should not be empty.
  DCHECK(!target_info || target_info->HasTargets());
  return target_info != nullptr;
}

bool AcceleratorManager::Process(const Accelerator& accelerator) {
  const AcceleratorTargetInfo* target_info = accelerators_.Find(accelerator);
  if (!target_info)
    return false;

  // If the accelerator is in the map, the target list should not be empty.
  DCHECK(target_info->HasTargets());

  // We have to copy the target list here, because processing the accelerator
  // event handler may modify the list.
  AcceleratorTargetInfo target_info_copy(*target_info);
  return target_info_copy.TryProcess(accelerator);
}

bool AcceleratorManager::HasPriorityHandler(
    const Accelerator& accelerator) const {
  const AcceleratorTargetInfo* target_info = accelerators_.Find(accelerator);
  return target_info && target_info->HasPriorityHandler();
}

AcceleratorManager::AcceleratorTargetInfo::AcceleratorTargetInfo() = default;

AcceleratorManager::AcceleratorTargetInfo::AcceleratorTargetInfo(
    const AcceleratorManager::AcceleratorTargetInfo& other) = default;

AcceleratorManager::AcceleratorTargetInfo&
AcceleratorManager::AcceleratorTargetInfo::operator=(
    const AcceleratorManager::AcceleratorTargetInfo& other) = default;

AcceleratorManager::AcceleratorTargetInfo::~AcceleratorTargetInfo() = default;

void AcceleratorManager::AcceleratorTargetInfo::RegisterWithPriority(
    AcceleratorTarget* target,
    HandlerPriority priority) {
  DCHECK(!Contains(target)) << "Registering the same target multiple times";

  // All priority accelerators go to the front of the line.
  if (priority == kHighPriority) {
    DCHECK(!has_priority_handler_)
        << "Only one high-priority handler can be registered";
    targets_.push_front(target);
    // Mark that we have a priority accelerator at the front.
    has_priority_handler_ = true;
  } else {
    // We are registering a normal priority handler. If no priority
    // accelerator handler has been registered before us, just add the new
    // handler to the front. Otherwise, register it after the first (only)
    // priority handler.
    if (has_priority_handler_) {
      DCHECK(!targets_.empty());
      targets_.insert(++targets_.begin(), target);
    } else {
      targets_.push_front(target);
    }
  }

  // Post condition. Ensure there's at least one target.
  DCHECK(!targets_.empty());
}

bool AcceleratorManager::AcceleratorTargetInfo::Unregister(
    AcceleratorTarget* target) {
  DCHECK(!targets_.empty());

  // Only one priority handler is allowed, so if we remove the first element we
  // no longer have a priority target.
  if (targets_.front() == target)
    has_priority_handler_ = false;

  // Attempt to remove the target and return true if it was present.
  const size_t original_target_count = targets_.size();
  targets_.remove(target);
  return original_target_count != targets_.size();
}

bool AcceleratorManager::AcceleratorTargetInfo::TryProcess(
    const Accelerator& accelerator) {
  DCHECK(!targets_.empty());

  for (AcceleratorTarget* target : targets_) {
    if (target->CanHandleAccelerators() &&
        target->AcceleratorPressed(accelerator)) {
      return true;
    }
  }

  return false;
}

bool AcceleratorManager::AcceleratorTargetInfo::HasPriorityHandler() const {
  DCHECK(!targets_.empty());
  return has_priority_handler_ && targets_.front()->CanHandleAccelerators();
}

bool AcceleratorManager::AcceleratorTargetInfo::Contains(
    AcceleratorTarget* target) const {
  DCHECK(target);
  return base::Contains(targets_, target);
}

}  // namespace ui
