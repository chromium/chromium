// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_H_

#include "base/component_export.h"
#include "base/memory/raw_ref.h"
#include "ui/accessibility/ax_mode.h"

namespace ui {

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

    // Sets the effective process-wide accessibility mode.
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

 private:
  // The embedder's delegate.
  const raw_ref<Delegate> delegate_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_H_
