// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_ACCELERATOR_MANAGER_H_
#define UI_BASE_ACCELERATORS_ACCELERATOR_MANAGER_H_

#include <list>
#include <map>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"

namespace ui {

// AcceleratorManger handles processing of accelerators. A delegate may be
// supplied which is notified as unique accelerators are added and removed.
class COMPONENT_EXPORT(UI_BASE) AcceleratorManager {
 public:
  enum HandlerPriority {
    kNormalPriority,
    kHighPriority,
  };

  AcceleratorManager();
  ~AcceleratorManager();

  // Register keyboard accelerators for the specified target. If multiple
  // targets are registered for an accelerator, a target registered later has
  // higher priority.
  // |accelerators| contains accelerators to register.
  // |priority| denotes the priority of the handler.
  // NOTE: In almost all cases, you should specify kNormalPriority for this
  // parameter. Setting it to kHighPriority prevents Chrome from sending the
  // shortcut to the webpage if the renderer has focus, which is not desirable
  // except for very isolated cases.
  // |target| is the AcceleratorTarget that handles the event once the
  // accelerator is pressed.
  // Note that we are currently limited to accelerators that are either:
  // - a key combination including Ctrl or Alt
  // - the escape key
  // - the enter key
  // - any F key (F1, F2, F3 ...)
  // - any browser specific keys (as available on special keyboards)
  void Register(const std::vector<ui::Accelerator>& accelerators,
                HandlerPriority priority,
                AcceleratorTarget* target);

  // Registers a keyboard accelerator for the specified target. This function
  // calls the function Register() with vector argument above.
  inline void RegisterAccelerator(const Accelerator& accelerator,
                                  HandlerPriority priority,
                                  AcceleratorTarget* target) {
    Register({accelerator}, priority, target);
  }

  // Unregister the specified keyboard accelerator for the specified target.
  void Unregister(const Accelerator& accelerator, AcceleratorTarget* target);

  // Unregister all keyboard accelerator for the specified target.
  void UnregisterAll(AcceleratorTarget* target);

  // Returns whether |accelerator| is already registered.
  bool IsRegistered(const Accelerator& accelerator) const;

  // Activates the target associated with the specified accelerator.
  // First, AcceleratorPressed handler of the most recently registered target
  // is called, and if that handler processes the event (i.e. returns true),
  // this method immediately returns. If not, we do the same thing on the next
  // target, and so on.
  // Returns true if an accelerator was activated.
  bool Process(const Accelerator& accelerator);

  // Whether the given |accelerator| has a priority handler associated with it.
  bool HasPriorityHandler(const Accelerator& accelerator) const;

 private:
  // The accelerators and associated targets.
  using AcceleratorTargetList = std::list<AcceleratorTarget*>;
  // This construct pairs together a |bool| (denoting whether the list contains
  // a priority_handler at the front) with the list of AcceleratorTargets.
  using AcceleratorTargets = std::pair<bool, AcceleratorTargetList>;
  using AcceleratorTargetsMap = std::map<Accelerator, AcceleratorTargets>;

  // Implementation of Unregister(). |map_iter| points to the accelerator to
  // remove, and |target| the AcceleratorTarget to remove.
  void UnregisterImpl(AcceleratorTargetsMap::iterator map_iter,
                      AcceleratorTarget* target);

  AcceleratorTargetsMap accelerators_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorManager);
};

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_ACCELERATOR_MANAGER_H_
