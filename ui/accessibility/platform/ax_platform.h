// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_H_

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ref.h"
#include "base/observer_list.h"
#include "base/scoped_observation_traits.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_mode.h"

namespace ui {

class AXModeObserver;
class AXPlatformNode;

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

  void AddModeObserver(AXModeObserver* observer);
  void RemoveModeObserver(AXModeObserver* observer);

  // Notifies observers that the mode flags in `mode` have been added to the
  // process-wide accessibility mode.
  void NotifyModeAdded(AXMode mode);

#if BUILDFLAG(IS_WIN)
  // Enables or disables use of the UI Automation Provider on Windows. If this
  // function is not called, the provider is enabled or disabled on the basis of
  // the "UiaProvider" base::Feature. In such cases, the `--enable-features` or
  // `--disable-features` switches on the browser's command line may be used to
  // enable or disable use of the provider, respectively. This function may only
  // be called during browser process startup before any UI is presented.
  void SetUiaProviderEnabled(bool is_enabled);

  // Returns true if the UI Automation Provider for Windows is enabled.
  bool IsUiaProviderEnabled() const;
#endif

 private:
  friend class ::ui::AXPlatformNode;
  FRIEND_TEST_ALL_PREFIXES(AXPlatformTest, Observer);

  // Sets the process-wide accessibility mode.
  void SetMode(AXMode new_mode) { delegate_->SetProcessMode(new_mode); }

  // The embedder's delegate.
  const raw_ref<Delegate> delegate_;

  base::ObserverList<AXModeObserver,
                     /*check_empty=*/true,
                     /*allow_reentrancy=*/false>
      observers_;

#if BUILDFLAG(IS_WIN)
  enum class UiaProviderEnablement {
    // Enabled or disabled via Chrome Variations (base::FeatureList).
    kVariations,
    // Explicitly enabled at runtime.
    kEnabled,
    // Explicitly disabled at runtime.
    kDisabled,
  };
  UiaProviderEnablement uia_provider_enablement_ =
      UiaProviderEnablement::kVariations;
#endif
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
