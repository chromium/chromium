// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_H_

#include "base/component_export.h"
#include "base/memory/raw_ref.h"
#include "base/observer_list.h"
#include "base/scoped_observation_traits.h"
#include "ui/accessibility/ax_mode.h"

namespace ui {

class AXModeObserver;

// Process-wide accessibility platform state.
class COMPONENT_EXPORT(AX_PLATFORM) AXPlatform {
 public:
  class COMPONENT_EXPORT(AX_PLATFORM) Delegate {
   public:
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    virtual ~Delegate() = default;

    // Returns the effective process-wide accessibility mode.
    virtual AXMode GetProcessMode() = 0;

    // Sets the effective process-wide accessibility mode and notifies observers
    // if `new_mode` contains additions to the mode flags.
    virtual void SetProcessMode(AXMode new_mode) = 0;

   protected:
    Delegate() = default;
  };

  // Returns the single process-wide instance.
  static AXPlatform& GetInstance();

  // Constructs a new instance. Only one instance may be alive in a process at
  // any given time. Typically, the embedder creates one during process startup
  // and ensures that it is kept alive throughout the process's UX.
  explicit AXPlatform(Delegate& delegate);
  AXPlatform(const AXPlatform&) = delete;
  AXPlatform& operator=(const AXPlatform&) = delete;
  ~AXPlatform();

  // Returns the process-wide accessibility mode.
  AXMode GetMode() { return delegate_->GetProcessMode(); }

  // Sets the process-wide accessibility mode.
  // TODO(grt): Remove all non-test callers and rename to SetModeForTesting.
  void SetMode(AXMode new_mode) { delegate_->SetProcessMode(new_mode); }

  void AddModeObserver(AXModeObserver* observer);
  void RemoveModeObserver(AXModeObserver* observer);

  // Notifies observers that the mode flags in `mode` have been added to the
  // process-wide accessibility mode.
  void NotifyModeAdded(AXMode mode);

 private:
  // The embedder's delegate.
  const raw_ref<Delegate> delegate_;

  base::ObserverList<AXModeObserver,
                     /*check_empty=*/true,
                     /*allow_reentrancy=*/false>
      observers_;
};

}  // namespace ui

namespace base {

// Traits type in support of base::ScopedObservation.
template <>
struct ScopedObservationTraits<ui::AXPlatform, ui::AXModeObserver> {
  static void AddObserver(ui::AXPlatform* source,
                          ui::AXModeObserver* observer) {
    source->AddModeObserver(observer);
  }
  static void RemoveObserver(ui::AXPlatform* source,
                             ui::AXModeObserver* observer) {
    source->RemoveModeObserver(observer);
  }
};

}  // namespace base

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_H_
