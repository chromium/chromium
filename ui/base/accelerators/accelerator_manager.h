// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_ACCELERATOR_MANAGER_H_
#define UI_BASE_ACCELERATORS_ACCELERATOR_MANAGER_H_

#include <list>
#include <map>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/accelerator_map.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/base/ui_base_features.h"
#endif

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
  AcceleratorManager(const AcceleratorManager& other) = delete;
  AcceleratorManager& operator=(const AcceleratorManager& other) = delete;
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
  // DCHECKs if |target| is null or not registered.
  void Unregister(const Accelerator& accelerator, AcceleratorTarget* target);

  // Unregister all keyboard accelerator for the specified target. DCHECKs if
  // |target| is null.
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

#if BUILDFLAG(IS_CHROMEOS)
  void SetUsePositionalLookup(bool use_positional_lookup) {
    DCHECK(::features::IsImprovedKeyboardShortcutsEnabled());
    accelerators_.set_use_positional_lookup(use_positional_lookup);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

 private:
  // Private helper class to manage the accelerator targets and priority. Each
  // set of targets for a given accelerator can only have 0 or 1 priority
  // handlers. If present this handler is tried first, otherwise all handlers
  // are tried in registration order.
  class AcceleratorTargetInfo {
   public:
    AcceleratorTargetInfo();
    AcceleratorTargetInfo(const AcceleratorTargetInfo& other);
    AcceleratorTargetInfo& operator=(const AcceleratorTargetInfo& other);
    ~AcceleratorTargetInfo();

    // Registers a |target| with a given |priority|.
    void RegisterWithPriority(AcceleratorTarget* target,
                              HandlerPriority priority);

    // Unregisters |target| if it exists. Returns true if |target| was present.
    bool Unregister(AcceleratorTarget* target);

    // Iterate through the targets in priority order attempting to process
    // |accelerator|. Returns true if a target processed |accelerator|.
    bool TryProcess(const Accelerator& accelerator);

    // Returns true if this set of targets has a priority handler and it can
    // currently handle an accelerator.
    bool HasPriorityHandler() const;

    // Returns true if |target| is registered. Note this is O(num_targets).
    bool Contains(AcceleratorTarget* target) const;

    // Returns true if there are registered targets.
    bool HasTargets() const { return !targets_.empty(); }

    // Returns the number of targets for this accelerator.
    size_t size() const { return targets_.size(); }

   private:
    std::list<raw_ptr<AcceleratorTarget, CtnExperimental>> targets_;
    bool has_priority_handler_ = false;
  };

  // The accelerators and associated targets.
  AcceleratorMap<AcceleratorTargetInfo> accelerators_;
};

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_ACCELERATOR_MANAGER_H_
